import os
import struct
import json
from typing import List, Tuple
from enum import Enum

PKG_NAME_LEN_MAX = 256     # 最后一个字节强制为"\0"
PKG_ID_LEN_MAX = 256       # 最后一个字节强制为"\0"
PKG_VERSION_LEN_MAX = 256  # 最后一个字节强制为"\0"

class ScriptType(Enum):
    APPLICATION = b"EAPK"
    WATCHFACE = b"EWPK"

def pack_string(s: str, max_length: int) -> bytes:
    """打包固定长度字符串，不足补0"""
    utf8_bytes = s.encode('utf-8')[:max_length-1]
    return utf8_bytes + b'\0' * (max_length - len(utf8_bytes))

def pack_entry(name: str, is_dir: bool, offset: int, size: int) -> bytes:
    name_utf8 = name.encode('utf-8')
    # 必须使用"<"，小端序，无 padding 自动填充
    return struct.pack(f"<I{len(name_utf8)}sIII", len(name_utf8), name_utf8, 1 if is_dir else 0, offset, size)

def calculate_table_size(entries: List[Tuple[str, bool, int]]) -> int:
    """计算文件表总大小"""
    total = 0
    for name, is_dir, size in entries:
        # 文件名长度(4字节) + 文件名 + 目录标记(4字节) + 偏移量(4字节) + 大小(4字节)
        name_len = len(name.encode('utf-8'))
        total += 4 + name_len + 12
    return total

def collect_files(directory: str) -> List[Tuple[str, bool, int]]:
    """收集目录下所有文件和子目录"""
    entries = []
    for root, dirs, files in os.walk(directory):
        rel_root = os.path.relpath(root, directory)
        if rel_root == ".":
            rel_root = ""
        
        for d in dirs:
            path = os.path.join(rel_root, d) if rel_root else d
            entries.append((path, True, 0))
        
        for f in files:
            path = os.path.join(rel_root, f) if rel_root else f
            full_path = os.path.join(root, f)
            size = os.path.getsize(full_path)
            entries.append((path, False, size))
    
    return entries

def read_manifest(input_dir: str) -> Tuple[str, str, str]:
    """从manifest.json读取包信息"""
    manifest_path = os.path.join(input_dir, 'manifest.json')
    if not os.path.exists(manifest_path):
        raise FileNotFoundError(f"manifest.json not found in {input_dir}")
    
    with open(manifest_path, 'r', encoding='utf-8') as f:
        try:
            manifest = json.load(f)
        except json.JSONDecodeError as e:
            raise ValueError(f"Invalid manifest.json: {e}")
    
    required_fields = ['id', 'name', 'version']
    for field in required_fields:
        if field not in manifest:
            raise ValueError(f"Missing required field in manifest.json: {field}")
    
    return manifest['name'], manifest['id'], manifest['version']

def pack_directory(input_dir: str, output_file: str, script_type: ScriptType):
    """打包目录为EAPK/EWPK文件(带包名、ID和版本号)"""
    entries = collect_files(input_dir)
    if not entries:
        raise ValueError("No files found in input directory")
    
    # 从manifest.json读取包信息
    pkg_name, pkg_id, pkg_version = read_manifest(input_dir)
    
    file_count = len(entries)
    
    # magic(4) + pkg_name(256) + pkg_id(256) + pkg_version(256) + file_count(4) + reserved(4)
    header_size = 4 + PKG_NAME_LEN_MAX + PKG_ID_LEN_MAX + PKG_VERSION_LEN_MAX + 8
    
    # 计算文件表大小
    table_size = calculate_table_size(entries)
    
    # 文件表位置
    table_offset = header_size
    
    # 文件数据偏移量
    data_start_offset = header_size + table_size
    
    # 计算每个文件的绝对偏移量
    current_offset = data_start_offset
    file_offsets = []
    for name, is_dir, size in entries:
        if not is_dir:
            file_offsets.append(current_offset)
            current_offset += size
        else:
            file_offsets.append(0)
    
    # 写入文件
    with open(output_file, 'wb') as f:
        # 写入包头
        f.write(script_type.value)  # magic
        f.write(pack_string(pkg_name, PKG_NAME_LEN_MAX))  # pkg_name
        f.write(pack_string(pkg_id, PKG_ID_LEN_MAX))  # pkg_id
        f.write(pack_string(pkg_version, PKG_VERSION_LEN_MAX))  # pkg_version
        f.write(struct.pack("<II", file_count, 0))  # file_count + reserved
        
        # 写入文件表
        for (name, is_dir, size), offset in zip(entries, file_offsets):
            f.write(pack_entry(name, is_dir, offset, size))
        
        # 写入文件数据
        for (name, is_dir, size), offset in zip(entries, file_offsets):
            if not is_dir:
                with open(os.path.join(input_dir, name), 'rb') as src:
                    f.write(src.read())

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Pack directory to EAPK/EWPK file')
    parser.add_argument('input_dir', help='Input directory to pack')
    parser.add_argument('output_file', help='Output package file')
    parser.add_argument('--type', choices=['app', 'watchface'], default='app',
                       help='Package type: app (EAPK) or watchface (EWPK)')
    
    args = parser.parse_args()
    
    script_type = ScriptType.APPLICATION if args.type == 'app' else ScriptType.WATCHFACE
    pack_directory(args.input_dir, args.output_file, script_type)
    print(f"Successfully packed {args.input_dir} to {args.output_file}")

if __name__ == '__main__':
    main()