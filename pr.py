import os

# Расширения файлов для включения
include_extensions = ('.cpp', '.h', '.glsl')
# Точные имена файлов для включения (без учета расширений)
include_exact_files = ('Makefile', 'CMakeLists.txt')

# Папки, которые нужно полностью игнорировать (кеш компилятора, сборка, репозиторий)
exclude_dirs = ('build', 'build_win', 'build_mingw', 'build_msvc', 'out', 'bin')
exclude_files = ('stb_image.h', 'stb_image_write.h', 'enet.h')
output_file = 'project_context.txt'

def generate_tree(dir_path, prefix=""):
    tree_str = ""
    try:
        files = sorted(os.listdir(dir_path))
    except Exception:
        return ""
        
    for i, file in enumerate(files):
        # Игнорируем скрытые файлы/папки, выходной файл и папки сборки
        if file.startswith('.') or file == output_file or file in exclude_dirs:
            continue
            
        path = os.path.join(dir_path, file)
        is_last = (i == len(files) - 1)
        connector = "└── " if is_last else "├── "
        
        tree_str += f"{prefix}{connector}{file}\n"
        
        if os.path.isdir(path):
            extension_prefix = "    " if is_last else "│   "
            tree_str += generate_tree(path, prefix + extension_prefix)
            
    return tree_str

with open(output_file, 'w', encoding='utf-8') as outfile:
    outfile.write("=== PROJECT STRUCTURE ===\n")
    outfile.write(".\n" + generate_tree('.'))
    outfile.write("\n=========================\n\n")
    
    for root, dirs, files in os.walk('.'):
        # Исключаем скрытые папки и папки сборки из дальнейшего обхода os.walk
        dirs[:] = [d for d in dirs if not d.startswith('.') and d not in exclude_dirs]
        
        for file in sorted(files):
            if file in exclude_files or file == output_file:
                continue
                
            is_valid = file.endswith(include_extensions) or file in include_exact_files
            
            if is_valid:
                file_path = os.path.join(root, file)
                outfile.write(f"\n--- FILE: {file_path} ---\n")
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as infile:
                        outfile.write(infile.read())
                except Exception as e:
                    outfile.write(f"Error reading file: {e}\n")