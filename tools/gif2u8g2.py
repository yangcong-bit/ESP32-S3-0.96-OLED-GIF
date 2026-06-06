#!/usr/bin/env python3
"""
GIF 转 U8g2 位图工具（通用版）

功能：
1. 读取 GIF 图片（支持动画 GIF 的每一帧）
2. 调整大小到指定分辨率（默认 128x64）
3. 二值化为黑白图像
4. 导出通用变量名的 C 语言头文件
5. 自动更新 main.c 中的引用
6. 可选：自动构建并烧录 ESP-IDF 项目

用法：
    python gif2u8g2.py input.gif [--build] [--flash]

依赖：
    pip install Pillow
"""

import argparse
import glob
import os
import re
import subprocess
import sys
from PIL import Image


# ==================== 内容检测 ====================

def analyze_gif_content(img, frame_count, threshold=128):
    """
    分析 GIF 所有帧的有效内容区域（bounding box）
    
    返回: (min_x, min_y, max_x, max_y) - 有效像素边界
    """
    # 初始化边界
    min_x, min_y = float('inf'), float('inf')
    max_x, max_y = 0, 0
    has_content = False
    
    for idx in range(frame_count):
        if frame_count > 1:
            img.seek(idx)
        frame = img.copy()
        
        # 转灰度
        if frame.mode != 'L':
            if frame.mode in ('RGBA', 'LA'):
                # 处理透明通道
                r, g, b, a = frame.split()
                # 将透明区域设为黑色
                frame_gray = frame.convert('L')
            else:
                frame_gray = frame.convert('L')
        else:
            frame_gray = frame
        
        # 二值化找到非零像素
        pixels = frame_gray.load()
        w, h = frame_gray.size
        
        for y in range(h):
            for x in range(w):
                if pixels[x, y] > threshold:
                    min_x = min(min_x, x)
                    min_y = min(min_y, y)
                    max_x = max(max_x, x)
                    max_y = max(max_y, y)
                    has_content = True
    
    if not has_content:
        # 没有有效内容，返回原始尺寸
        w, h = img.size
        return (0, 0, w - 1, h - 1)
    
    # 加一点边距（各边加2像素）
    min_x = max(0, min_x - 2)
    min_y = max(0, min_y - 2)
    max_x = min(img.size[0] - 1, max_x + 2)
    max_y = min(img.size[1] - 1, max_y + 2)
    
    return (min_x, min_y, max_x, max_y)


def calc_optimal_size(bbox, max_width=128, max_height=64):
    """
    根据内容边界计算最优输出尺寸
    
    保持宽高比，适配到最大分辨率
    """
    content_w = bbox[2] - bbox[0] + 1
    content_h = bbox[3] - bbox[1] + 1
    
    # 计算缩放比例（保持比例，适配最大尺寸）
    scale_w = max_width / content_w
    scale_h = max_height / content_h
    scale = min(scale_w, scale_h)
    
    # 计算输出尺寸（偶数，方便 SSD1306 分页）
    out_w = int(content_w * scale)
    out_h = int(content_h * scale)
    
    # 确保是8的倍数（SSD1306 页对齐）
    out_w = (out_w // 8) * 8
    out_h = (out_h // 8) * 8
    
    # 最小尺寸限制
    out_w = max(8, min(out_w, max_width))
    out_h = max(8, min(out_h, max_height))
    
    return out_w, out_h, scale


# ==================== 图像处理 ====================

def resize_image(img, target_width=128, target_height=64, crop_box=None):
    """调整图片尺寸，保持宽高比，居中放置在黑色背景上"""
    if crop_box:
        # 裁剪到有效内容区域
        img = img.crop(crop_box)
    
    orig_width, orig_height = img.size
    scale = min(target_width / orig_width, target_height / orig_height)
    new_w = int(orig_width * scale)
    new_h = int(orig_height * scale)
    img_resized = img.resize((new_w, new_h), Image.Resampling.LANCZOS)
    canvas = Image.new('RGB', (target_width, target_height), (0, 0, 0))
    canvas.paste(img_resized, ((target_width - new_w) // 2, (target_height - new_h) // 2))
    return canvas


def binarize_image(img, threshold=128, invert=False):
    """将图像二值化为黑白"""
    img_gray = img.convert('L') if img.mode != 'L' else img
    if invert:
        return img_gray.point(lambda x: 255 if x < threshold else 0, '1')
    else:
        return img_gray.point(lambda x: 255 if x >= threshold else 0, '1')


def image_to_u8g2_data(img, target_width=128, target_height=64):
    """
    将二值化图像转换为 SSD1306 页格式字节数组（自动居中填充到目标尺寸）
    
    输出始终是 target_width x target_height 的完整数据
    """
    src_w, src_h = img.size
    src_pixels = img.load()
    
    # 计算居中偏移
    offset_x = (target_width - src_w) // 2
    offset_y = (target_height - src_h) // 2
    
    pages = target_height // 8
    data = []
    
    for page in range(pages):
        for col in range(target_width):
            byte_val = 0
            for bit in range(8):
                row = page * 8 + bit
                # 映射到源图像坐标
                src_col = col - offset_x
                src_row = row - offset_y
                # 检查是否在源图像范围内
                if (0 <= src_col < src_w and 0 <= src_row < src_h 
                    and src_pixels[src_col, src_row]):
                    byte_val |= (1 << bit)
            data.append(byte_val)
    
    return data


# ==================== C 代码生成 ====================

def generate_header(frame_data, frame_count, width, height):
    """生成通用变量名的 C 头文件"""
    frame_size = (height // 8) * width

    lines = [
        "/**",
        " * @file gif_frames.h",
        " * @brief 自动生成的 GIF 动画帧数据",
        f" * 帧数: {frame_count}, 尺寸: {width}x{height}, 每帧: {frame_size} 字节",
        " */",
        "",
        "#ifndef GIF_FRAMES_H",
        "#define GIF_FRAMES_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define GIF_FRAME_WIDTH   {width}",
        f"#define GIF_FRAME_HEIGHT  {height}",
        f"#define GIF_FRAME_COUNT   {frame_count}",
        f"#define GIF_FRAME_SIZE    {frame_size}",
        "",
        "static const uint8_t gif_frame_data[] = {",
    ]

    for i in range(0, len(frame_data), 16):
        chunk = frame_data[i:i+16]
        hex_vals = ', '.join(f'0x{b:02X}' for b in chunk)
        comma = "," if i + 16 < len(frame_data) else ""
        lines.append(f"    {hex_vals}{comma}")

    lines.append("};")
    lines.append("")
    lines.append("#endif /* GIF_FRAMES_H */")

    return '\n'.join(lines)


# ==================== main.c 自动更新 ====================

def update_main_c(main_path, frame_count):
    """更新 main.c，确保使用通用变量名"""
    if not os.path.exists(main_path):
        print(f"  ⚠ 未找到 {main_path}，跳过")
        return False

    with open(main_path, 'r', encoding='utf-8') as f:
        content = f.read()

    updated = False

    # 替换 include
    new_content = re.sub(
        r'#include\s+"gif_frames\.h"',
        '#include "gif_frames.h"',
        content
    )
    if new_content != content:
        content = new_content
        updated = True

    # 替换帧数据引用 → gif_frame_data
    new_content = re.sub(
        r'(?:bitmap_[^\s*]+\s*_data|gif_frame_data)\[',
        'gif_frame_data[',
        content
    )
    if new_content != content:
        content = new_content
        updated = True

    # 替换帧指针数组引用 → gif_frames
    new_content = re.sub(
        r'(?:bitmap_[^\s*]+\s*\[|gif_frames\[)',
        'gif_frames[',
        content
    )
    if new_content != content:
        content = new_content
        updated = True

    # 替换帧数宏 → GIF_FRAME_COUNT
    new_content = re.sub(
        r'%\s*(?:BITMAP_[A-Z_]+_FRAME_COUNT|GIF_FRAME_COUNT)',
        '% GIF_FRAME_COUNT',
        content
    )
    if new_content != content:
        content = new_content
        updated = True

    # 替换 ESP_LOGI 中的帧数
    new_content = re.sub(
        r'ESP_LOGI\(TAG,\s*"GIF player started[^"]*"\)',
        f'ESP_LOGI(TAG, "GIF player started, %d frames", GIF_FRAME_COUNT)',
        content
    )
    if new_content != content:
        content = new_content
        updated = True

    if updated:
        with open(main_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  ✅ 已更新 {main_path}")
    else:
        print(f"  ℹ {main_path} 无需更新")

    return True


# ==================== ESP-IDF 构建与烧录 ====================

def run_idf(project_dir, args_list, description):
    """执行 idf.py 命令"""
    print(f"\n{'='*50}")
    print(f"  {description}")
    print(f"{'='*50}")

    cmd = ['idf.py'] + args_list
    print(f"  命令: {' '.join(cmd)}")
    print(f"  目录: {project_dir}\n")

    result = subprocess.run(cmd, cwd=project_dir)

    if result.returncode != 0:
        print(f"\n  ❌ {description} 失败 (退出码: {result.returncode})")
        return False

    print(f"\n  ✅ {description} 完成")
    return True


# ==================== GIF 文件查找 ====================

def find_gif_files(pattern):
    """
    根据通配符或目录查找 GIF 文件
    
    支持：
    - 具体文件名: input.gif
    - 通配符: *.gif, anim*.gif, cat?.gif
    - 目录路径: ./gifs/ （查找目录下所有 GIF）
    - 不带参数: 查找当前目录所有 GIF
    """
    # 如果是目录，查找目录下所有 GIF
    if os.path.isdir(pattern):
        gif_files = sorted(glob.glob(os.path.join(pattern, '*.gif')))
        if gif_files:
            return gif_files
        # 也查找大写扩展名
        gif_files.extend(sorted(glob.glob(os.path.join(pattern, '*.GIF'))))
        return gif_files
    
    # 使用 glob 通配符展开
    matched = sorted(glob.glob(pattern))
    
    # 过滤只保留 GIF 文件
    gif_files = [f for f in matched if f.lower().endswith('.gif') and os.path.isfile(f)]
    
    return gif_files


# ==================== 主函数 ====================

def main():
    parser = argparse.ArgumentParser(
        description='GIF 转 U8g2 位图工具（通用版）',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例：
  %(prog)s input.gif                      # 转换指定文件
  %(prog)s *.gif                          # 转换所有 GIF
  %(prog)s anim*.gif --build              # 通配符 + 构建
  %(prog)s ./gifs/                        # 转换目录下所有 GIF
  %(prog)s                                # 自动查找当前目录所有 GIF
  %(prog)s -i --build --flash             # 反转 + 构建 + 烧录
        """
    )

    parser.add_argument('input', nargs='?', default='*.gif',
                        help='GIF 文件路径（支持通配符，默认: *.gif）')
    parser.add_argument('-o', '--output', help='输出 .h 文件（默认: main/gif_frames.h）')
    parser.add_argument('-w', '--width', type=int, default=128, help='目标宽度（默认: 128）')
    parser.add_argument('-height', '--height', type=int, default=64, help='目标高度（默认: 64）')
    parser.add_argument('-t', '--threshold', type=int, default=128, help='二值化阈值（默认: 128）')
    parser.add_argument('-i', '--invert', action='store_true', help='反转颜色')
    parser.add_argument('-a', '--auto', action='store_true', help='自动检测内容区域并适配尺寸')
    parser.add_argument('--build', action='store_true', help='转换后自动构建')
    parser.add_argument('--flash', action='store_true', help='构建后自动烧录')
    parser.add_argument('--project-dir', default='.', help='ESP-IDF 项目目录')

    args = parser.parse_args()

    # 查找匹配的 GIF 文件
    gif_files = find_gif_files(args.input)

    if not gif_files:
        print(f"❌ 未找到匹配的 GIF 文件: {args.input}")
        print(f"   提示: 请确保当前目录下有 .gif 文件，或指定正确的路径/通配符")
        return 1

    # 如果只有一个文件，直接处理
    if len(gif_files) == 1:
        gif_files = [gif_files[0]]

    print(f"找到 {len(gif_files)} 个 GIF 文件:")
    for f in gif_files:
        print(f"  - {f}")

    # 默认输出路径
    if args.output is None:
        args.output = os.path.join(args.project_dir, 'main', 'gif_frames.h')

    # 处理每个 GIF 文件
    for gif_path in gif_files:
        print(f"\n{'='*50}")
        print(f"处理: {gif_path}")
        print(f"{'='*50}")

        try:
            img = Image.open(gif_path)
            frame_count = getattr(img, 'n_frames', 1)
            print(f"  帧数: {frame_count}, 原始尺寸: {img.size}")

            # 自动检测内容区域
            target_w, target_h = args.width, args.height
            crop_box = None
            
            if args.auto:
                print(f"  正在分析内容区域...")
                bbox = analyze_gif_content(img, frame_count, args.threshold)
                content_w = bbox[2] - bbox[0] + 1
                content_h = bbox[3] - bbox[1] + 1
                print(f"  有效内容区域: ({bbox[0]},{bbox[1]}) - ({bbox[2]},{bbox[3]}) = {content_w}x{content_h}")
                
                # 计算最优尺寸
                target_w, target_h, scale = calc_optimal_size(bbox, args.width, args.height)
                crop_box = bbox
                print(f"  适配输出尺寸: {target_w}x{target_h} (缩放: {scale:.2f}x)")

            all_data = []
            for idx in range(frame_count):
                if frame_count > 1:
                    img.seek(idx)
                frame = img.copy()

                # 转 RGB
                if frame.mode in ('RGBA', 'LA'):
                    bg = Image.new('RGB', frame.size, (0, 0, 0))
                    bg.paste(frame, mask=frame.split()[-1])
                    frame = bg
                elif frame.mode == 'P':
                    rgba = frame.convert('RGBA')
                    bg = Image.new('RGB', frame.size, (0, 0, 0))
                    bg.paste(rgba, mask=rgba.split()[-1])
                    frame = bg
                elif frame.mode != 'RGB':
                    frame = frame.convert('RGB')

                resized = resize_image(frame, target_w, target_h, crop_box)
                binary = binarize_image(resized, args.threshold, args.invert)
                data = image_to_u8g2_data(binary, 128, 64)
                all_data.extend(data)
                print(f"  帧 {idx+1}/{frame_count}: {len(data)} 字节")

            # 生成输出文件名（基于输入文件名）
            if len(gif_files) > 1:
                # 多个文件时，每个生成单独的头文件
                base_name = os.path.splitext(os.path.basename(gif_path))[0]
                output_path = os.path.join(args.project_dir, 'main', f'gif_{base_name}.h')
            else:
                output_path = args.output

            # 生成头文件（输出始终是 128x64）
            frame_size = (64 // 8) * 128  # 1024 字节
            header = generate_header(all_data, frame_count, 128, 64)
            with open(output_path, 'w', encoding='utf-8') as f:
                f.write(header)

            actual_frames = len(all_data) // frame_size
            print(f"\n  ✅ 已导出: {output_path}")
            print(f"     帧数: {actual_frames}, 每帧: {frame_size} 字节, 总计: {len(all_data)} 字节")
            print(f"     显示尺寸: 128x64 (内容自动居中)")

        except Exception as e:
            print(f"  ❌ 处理失败: {e}")
            continue

    # 更新 main.c（只处理第一个文件）
    main_c = os.path.join(args.project_dir, 'main', 'main.c')
    update_main_c(main_c, frame_count if 'frame_count' in dir() else 0)

    print(f"\n{'='*50}")
    print(f"✅ 所有 GIF 处理完成!")
    print(f"{'='*50}")

    # 构建
    if args.build:
        if not run_idf(args.project_dir, ['build'], '构建项目'):
            return 1

    # 烧录
    if args.flash:
        if not run_idf(args.project_dir, ['flash'], '烧录到设备'):
            return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
