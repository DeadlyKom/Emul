import os
import json

def first_bit(byte: int, pixel_value: int):
    """
    Найти первый бит слева, равный pixel_value (0 или 1)
    """
    mask = 0x80
    for bit in range(8):
        bit_val = 1 if (byte & mask) else 0
        if bit_val == pixel_value:
            return bit
        mask >>= 1
    return None

def last_bit(byte: int, pixel_value: int) -> int:
    """
    Найти последний бит справа, равный pixel_value (0 или 1)
    """
    mask = 0x01
    for bit in range(7, -1, -1):
        bit_val = 1 if (byte & (1 << bit)) else 0
        if bit_val == pixel_value:
            return bit
    return None

def to_rows(sprite: bytes, width_bytes: int, height: int):
    """
    разрезделение спрайта на строки
    """
    rows = []

    for y in range(height):
        start = y * width_bytes
        end = start + width_bytes
        rows.append(sprite[start:end])

    return rows

def analyze_sprite(sprite: list[bytes], width_bytes: int, height: int,
                   left_padding: int = 0, pixel_value: int = 0) -> dict:
    """
    Анализ спрайта
        sprite - спрайт (маска)
        width_bytes - ширина спрайта в байтах
        height - высота спрайта в строках
        left_padding - отступ слева после сдвига
        pixel_value - какой бит считать пикселем (0 или 1)
    """

    min_y = height
    max_y = -1

    # --- вертикальный bound
    for y in range(height):
        row = sprite[y]
        for xb in range(width_bytes):
            b = row[xb]
            if (pixel_value == 0 and b != 0xFF) or (pixel_value == 1 and b != 0x00):
                min_y = min(min_y, y)
                max_y = max(max_y, y)
                break

    if max_y == -1:
        return None  # пустой спрайт

    # --- горизонтальный bound
    min_x = width_bytes * 8
    max_x = -1

    for y in range(min_y, max_y + 1):
        row = sprite[y]

        # слева
        for xb in range(width_bytes):
            b = row[xb]
            bit = first_bit(b, pixel_value)
            if bit is not None:
                x = xb * 8 + bit
                min_x = min(min_x, x)
                break

        # справа
        for xb in reversed(range(width_bytes)):
            b = row[xb]
            bit = last_bit(b, pixel_value)
            if bit is not None:
                x = xb * 8 + (7- bit)
                max_x = max(max_x, x)
                break

    width = max_x - min_x + 1
    height = max_y - min_y + 1
    shift_pixels = max(min_x - left_padding, 0)

    return {
        "shift_pixels": shift_pixels,           # Количество пикселей, на которое можно сдвинуть спрайт влево с учётом left_padding.
        "min_x": min_x,                         # Левая граница спрайта в пикселях (по маске). Первый пиксель спрайта.
        "max_x": max_x,                         # Правая граница спрайта в пикселях (по маске). Последний пиксель спрайта.
        "min_y": min_y,                         # Верхняя граница спрайта (по маске), строка с первым пикселем.
        "max_y": max_y,                         # Нижняя граница спрайта (по маске), строка с последним пикселем.
        "width": width,                         # Ширина спрайта в пикселях внутри найденного bounding box: max_x - min_x + 1
        "height": height                        # Высота спрайта в пикселях внутри bounding box: max_y - min_y + 1
    }

def main():
    BASE_DIR = os.path.dirname(os.path.abspath(__file__))
    json_path = os.path.join(BASE_DIR, "Export.json")

    with open(json_path, "r", encoding="utf-8") as file:
        export = json.load(file)

    # основной цикл спрайтов
    for sprite in export:
        sprite_name = sprite["SprName"]
        sprite_width = sprite["SprWidth"]
        sprite_height = sprite["SprHeight"]

        # Загрузка бинарных данных
        with open(sprite["InkData"], "rb") as f: ink_data = bytearray(f.read())
        with open(sprite["AttributeData"], "rb") as f: attribute_data = bytearray(f.read())
        with open(sprite["MaskData"], "rb") as f: mask_data = bytearray(f.read())

        width_bytes = sprite_width // 8
        mask_rows = to_rows(mask_data, width_bytes, sprite_height)

        info = analyze_sprite(
            mask_rows,
            width_bytes,
            sprite_height,
            left_padding=0,
            pixel_value=1
        )

        print(sprite_name, info)    

if __name__ == "__main__":
    main()