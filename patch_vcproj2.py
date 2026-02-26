import codecs, re
path = r'c:\sw\XM62022\mfc\XM6.vcproj'
try:
    with codecs.open(path, 'r', 'shift_jis') as f:
        text = f.read()
except:
    with codecs.open(path, 'r', 'utf-8') as f:
        text = f.read()

if 'mfc_dx9.cpp' not in text:
    text = re.sub(
        r'(<File\s+RelativePath="\.\\mfc_draw\.cpp"\s*>\s*</File>)',
        r'\1\n\t\t\t\t<File\n\t\t\t\t\tRelativePath=".\\mfc_dx9.cpp"\n\t\t\t\t\t>\n\t\t\t\t</File>',
        text, flags=re.IGNORECASE)

if 'mfc_dx9.h' not in text:
    text = re.sub(
        r'(<File\s+RelativePath="\.\\mfc_draw\.h"\s*>\s*</File>)',
        r'\1\n\t\t\t\t<File\n\t\t\t\t\tRelativePath=".\\mfc_dx9.h"\n\t\t\t\t\t>\n\t\t\t\t</File>',
        text, flags=re.IGNORECASE)

with codecs.open(path, 'w', 'shift_jis') as f:
    f.write(text)
print('VCProj patched')
