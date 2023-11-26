#!python3

# Example script to decode an binary file encoded according to the schema into an image.
# Usage
# python3 image-encoder.py input_file.bin output_image.png

from PIL import Image
import io
import sys

SCREEN_WIDTH = 960
SCREEN_HEIGHT = 540

def read_global_header(f: io.BufferedReader):
    version = int.from_bytes(f.read(1), byteorder='little', signed=False)
    identifier = int.from_bytes(f.read(4), byteorder='little', signed=False)
    sleep_time = int.from_bytes(f.read(4), byteorder='little', signed=False)
    return (version, identifier, sleep_time)

def read_image_dimension(f: io.BufferedReader):
    firstBytes = f.read(2)
    if (len(firstBytes) < 2):
        return None

    x = int.from_bytes(firstBytes, byteorder='little', signed=False)
    y = int.from_bytes(f.read(2), byteorder='little', signed=False)
    width = int.from_bytes(f.read(2), byteorder='little', signed=False)
    height = int.from_bytes(f.read(2), byteorder='little', signed=False)
    return (x, y, width, height)

def main():
    if len(sys.argv) <= 2:
        print(f"Usage: {sys.argv[0]} <input> <output>")
        exit(1)

    input_file = sys.argv[1]
    output_image = sys.argv[2]

    image = Image.new('L', (SCREEN_WIDTH, SCREEN_HEIGHT), 255)
    image = image.convert(mode='L')

    with open(input_file, 'rb') as f:
        (version, identifier, sleep_time) = read_global_header(f)
        if version != 1:
            print(f"Recieved unkown schema version: {version}")
            exit(1)

        print(f"Read file with schema version {version}, identifier: {identifier} and sleep time: {sleep_time}s")

        while (dimension := read_image_dimension(f)):
            (x0, y0, width, height) = dimension
            print(f"Found image with dimensions: {dimension}")
            for y in range(y0, height):
                for x in range(x0, width, 2):
                    byteArray = f.read(1)
                    if byteArray is None:
                        print(f"Recieved unexpected end of file stream")
                        exit(1)

                    #  x2   x1
                    # 0000.0000
                    byte = byteArray[0]
                    first_nibble = (byte & 0x0F) << 4
                    second_nibble = byte & 0xF0
                    
                    image.putpixel((x, y), first_nibble)
                    image.putpixel((x + 1, y), second_nibble)
        
        image.save(output_image)

if __name__ == "__main__":
    main()