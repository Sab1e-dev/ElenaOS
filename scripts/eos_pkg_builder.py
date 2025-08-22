import os
import struct
from typing import List, Tuple
from enum import Enum

class ScriptType(Enum):
    APPLICATION = "EAPK"
    WATCHFACE = "EWPK"

class EosPkgHeader:
    def __init__(self, file_count: int, table_offset: int, script_type: ScriptType):
        self.magic = script_type.value.encode('ascii')
        self.version = 0x0001
        self.file_count = file_count
        self.table_offset = table_offset
        self.reserved = 0
    
    def pack(self) -> bytes:
        return struct.pack(
            "<4sIIII",
            self.magic,
            self.version,
            self.file_count,
            self.table_offset,
            self.reserved
        )

def pack_string(s: str) -> bytes:
    """打包可变长度字符串"""
    utf8_bytes = s.encode('utf-8')
    return struct.pack(f"I{len(utf8_bytes)}s", len(utf8_bytes), utf8_bytes)

def pack_entry(name: str, is_dir: bool, offset: int, size: int) -> bytes:
    """打包单个文件条目"""
    name_part = pack_string(name)
    return name_part + struct.pack("<III", 1 if is_dir else 0, offset, size)

def calculate_table_size(entries: List[Tuple[str, bool, int]]) -> int:
    """计算文件表总大小"""
    total = 0
    for name, is_dir, size in entries:
        total += 4 + len(name.encode('utf-8')) + 4 * 3

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

def pack_directory(input_dir: str, output_file: str, script_type: ScriptType):
    """打包目录为EAPK/EWPK文件(修复偏移量版)"""
    entries = collect_files(input_dir)
    file_count = len(entries)
    
    # 计算各部分大小
    header_size = 20  # 包头20字节(16+4保留字段)
    table_size = calculate_table_size(entries)
    
    # 计算文件数据偏移量(从文件开头计算)
    data_start_offset = header_size + table_size
    
    # 第一遍: 计算每个文件的绝对偏移量
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
        # 写入包头(包含正确的table_offset)
        header = EosPkgHeader(file_count, header_size, script_type)  # table_offset=20
        f.write(header.pack())  # 写入20字节(16+4)
        
        # 写入文件表
        for (name, is_dir, size), offset in zip(entries, file_offsets):
            f.write(pack_entry(name, is_dir, offset, size))
        
        # 写入文件数据
        for (name, is_dir, size), offset in zip(entries, file_offsets):
            if not is_dir:
                with open(os.path.join(input_dir, name), 'rb') as src:
                    while True:
                        data = src.read(4096)
                        if not data:
                            break
                        f.write(data)

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Pack directory to EAPK/EWPK file (fixed offset version)')
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