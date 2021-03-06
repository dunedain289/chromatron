
import os
import shutil
import zipfile

def clean():
    for f in ['chromatron_fw.zip']:
        try:
            os.remove(f)
        except OSError:
            pass


def build_wifi():
    cwd = os.getcwd()

    os.chdir('src/chromatron_wifi')

    os.system('python make_esp_firmware.py')

    shutil.copy('chromatron_wifi_fw.zip', cwd)

    os.chdir(cwd)


def build_main():
    os.system('python build.py')

if __name__ == '__main__':
    clean()
    build_main()
    build_wifi()
