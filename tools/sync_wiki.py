import os
import shutil
import subprocess
import tempfile
import sys

def main():
    wiki_url = 'https://github.com/andarazoroflove/DufferPro.wiki.git'
    wiki_src = os.path.abspath('wiki')

    print(f"Checking access to {wiki_url}...")
    res = subprocess.run(['git', 'ls-remote', wiki_url], capture_output=True, text=True)
    if res.returncode != 0:
        print("Note: The GitHub Wiki repository has not yet been initialized.")
        print("Please visit: https://github.com/andarazoroflove/DufferPro/wiki and click 'Create the first page'.")
        print("Once initialized on GitHub, run this script again to automatically sync all pages.")
        sys.exit(0)

    with tempfile.TemporaryDirectory() as tmp_dir:
        print(f"Cloning wiki repository to temporary directory...")
        subprocess.run(['git', 'clone', wiki_url, tmp_dir], check=True)

        for fname in os.listdir(wiki_src):
            if fname.endswith('.md'):
                src_path = os.path.join(wiki_src, fname)
                dst_path = os.path.join(tmp_dir, fname)
                shutil.copy2(src_path, dst_path)
                print(f"Copied {fname} to wiki")

        # Copy screenshots if needed
        docs_screenshots = os.path.abspath('docs/screenshots')
        if os.path.exists(docs_screenshots):
            wiki_images = os.path.join(tmp_dir, 'images')
            os.makedirs(wiki_images, exist_ok=True)
            for img in os.listdir(docs_screenshots):
                if img.endswith('.png'):
                    shutil.copy2(os.path.join(docs_screenshots, img), os.path.join(wiki_images, img))

        subprocess.run(['git', 'add', '.'], cwd=tmp_dir, check=True)
        diff = subprocess.run(['git', 'status', '--porcelain'], cwd=tmp_dir, capture_output=True, text=True)
        if diff.stdout.strip():
            subprocess.run(['git', 'commit', '-m', 'Update Duffer Pro Wiki and User Manual for v0.77-noonan'], cwd=tmp_dir, check=True)
            subprocess.run(['git', 'push', 'origin', 'master'], cwd=tmp_dir, check=True)
            print("Successfully pushed wiki documentation!")
        else:
            print("Wiki is already up to date.")

if __name__ == '__main__':
    main()

