import codecs
import re

path = r"c:\sw\XM62022\mfc\XM6.vcproj"
try:
    with codecs.open(path, "r", "utf-16") as f:
        text = f.read()
except UnicodeError:
    with codecs.open(path, "r", "utf-8") as f:
        text = f.read()

if "mfc_dx9.cpp" not in text:
    text = re.sub(
        r'(<File[\s\r\n]*RelativePath="\.\\mfc_draw\.cpp".*?</File>)',
        r'\1\r\n\t\t\t<File\r\n\t\t\t\tRelativePath=".\\mfc_dx9.cpp"\r\n\t\t\t\t>\r\n\t\t\t</File>',
        text,
        flags=re.DOTALL | re.IGNORECASE
    )

if "mfc_dx9.h" not in text:
    text = re.sub(
        r'(<File[\s\r\n]*RelativePath="\.\\mfc_draw\.h".*?</File>)',
        r'\1\r\n\t\t\t<File\r\n\t\t\t\tRelativePath=".\\mfc_dx9.h"\r\n\t\t\t\t>\r\n\t\t\t</File>',
        text,
        flags=re.DOTALL | re.IGNORECASE
    )

with codecs.open(path, "w", "utf-16") as f:
    f.write(text)

print("Patching done.")
