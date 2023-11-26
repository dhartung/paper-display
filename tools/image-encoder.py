#!python3

# Example script to encode in image into the required schema format.
# Usage
# python3 image-encoder.py input_image.png binary_output.bin

from PIL import Image
import sys

SCREEN_WIDTH = 960
SCREEN_HEIGHT = 540

def get_header(img: Image):
    version = 1
    sleep_time = 6000 # 10 minutes
    x = 0
    y = 0
    identifier = 0
    width = img.width
    height = img.height

    return [
        *version.to_bytes(1, "little"),
        *identifier.to_bytes(4, "little"),
        *sleep_time.to_bytes(4, "little"),
        *x.to_bytes(2, "little"),
        *y.to_bytes(2, "little"),
        *width.to_bytes(2, "little"),
        *height.to_bytes(2, "little")
    ]

def main():
    if len(sys.argv) <= 2:
        print(f"Usage: {sys.argv[0]} <input> <output>")
        exit(1)

    input_image = sys.argv[1]
    output_file = sys.argv[2]

    if SCREEN_WIDTH % 2:
        print("image width must be even!", file=sys.stderr)
        sys.exit(1)

    image = Image.open(input_image)
    image = image.convert(mode='L')
    if (image.width > SCREEN_WIDTH):
        print("Image too wide!")
        exit(1)

    if (image.height > SCREEN_HEIGHT):
        print("Image too tall!")
        exit(1)

    # Write out the output file.
    with open(output_file, 'wb') as f:
        result = []
        result.extend(get_header(image))

        for y in range(0, image.size[1]):
            byte = 0
            for x in range(0, image.size[0]):
                color = image.getpixel((x, y))
                if x % 2 == 0:
                    byte = color >> 4
                else:
                    byte |= color & 0xF0
                    result.append(byte)

        f.write(bytes(result))
        print(f"Finished writing {len(result)} bytes to {output_file}")

if __name__ == "__main__":
    main()