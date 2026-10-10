import urllib.request
import json
import os
import sys

import subprocess

def get_github_token():
    token = os.environ.get('GITHUB_TOKEN')
    if token:
        return token
    try:
        p = subprocess.Popen(['git', 'credential', 'fill'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        out, _ = p.communicate(input="protocol=https\nhost=github.com\n\n")
        for line in out.splitlines():
            if line.startswith('password='):
                return line.split('password=', 1)[1].strip()
    except Exception:
        pass
    return None

def main():
    token = get_github_token()
    if not token:
        print("Error: GitHub token not found in GITHUB_TOKEN or git credential helper.")
        sys.exit(1)

    repo = 'andarazoroflove/DufferPro'

    headers = {
        'Authorization': f'token {token}',
        'User-Agent': 'Duffer-Release-Bot',
        'Accept': 'application/vnd.github.v3+json'
    }

    body_md = """## Duffer Pro: Championship Edition – Release v0.75-noonan ⛳

### 🌟 Highlights
- **Single Unified Release Bundle:** Ready-to-run builds for **Windows PC**, **macOS Sonoma** (Universal 2: Apple Silicon + Intel), and bare-metal **Game Boy Advance** (`.gba` + `.sav`).
- **Three Environmental Themes:**
  - **Bushwood:** Classic country club, caddyshack crew, and Carl Spackler vs. the Gopher.
  - **La Playa:** Desert plateau, giant cacti, rolling tumbleweeds, rapids, Burning Man effigy, and dancing raver vs. police cart.
  - **Nocturnal Green:** Gothic Transylvanian castle, autumn foliage, coffin rock piles, dark lagoons, sea monster, zombies, and Ash with his boomstick.
- **Enhanced Physics & Aiming:**
  - Continuous sub-pixel coordinate inversion for laser-accurate mouse aiming.
  - Complete obstacle launch immunity (punch cleanly out of trees, bushes, or boulder tiles without getting trapped).
  - 3-click precision swing gauge with power auto-bounce.
- **Customization & Quick Play:**
  - **Sneak On Mode:** Randomized rounds with patrol cart ranger encounters.
  - **Play a Hole Mode:** 1–5 density sliders for Trees, Bushes, Rocks, and Water with strict 70% natural limit safety clamp.
  - **Replay Hole:** Instant one-click restart for current hole layout.

### 📦 Included in Release:
- `Duffer_Pro_v0.75-noonan.zip` — All-in-one unified release archive.

### 📖 User Manual & Documentation
Full gameplay instructions, club tables, and course architect guides are in the [Official User Manual](https://github.com/andarazoroflove/DufferPro/blob/main/wiki/User-Manual.md).
"""

    # Check if release already exists
    req_list = urllib.request.Request(f'https://api.github.com/repos/{repo}/releases', headers=headers)
    existing_id = None
    upload_url_template = None
    try:
        with urllib.request.urlopen(req_list) as resp:
            releases = json.loads(resp.read().decode('utf-8'))
            for r in releases:
                if r.get('tag_name') in ['v0.75-noonan', 'v0.7.5']:
                    existing_id = r['id']
                    upload_url_template = r['upload_url']
                    print(f"Found existing release ID: {existing_id}")
                    break
    except Exception as e:
        print("List releases exception:", e)

    if not existing_id:
        rel_payload = {
            'tag_name': 'v0.75-noonan',
            'target_commitish': 'main',
            'name': 'Duffer Pro v0.75-noonan (SemVer v0.7.5)',
            'body': body_md,
            'draft': False,
            'prerelease': False
        }
        req = urllib.request.Request(
            f'https://api.github.com/repos/{repo}/releases',
            data=json.dumps(rel_payload).encode('utf-8'),
            headers=headers,
            method='POST'
        )
        with urllib.request.urlopen(req) as resp:
            rel_data = json.loads(resp.read().decode('utf-8'))
            existing_id = rel_data['id']
            upload_url_template = rel_data['upload_url']
            print(f"Release created successfully! ID: {existing_id}")

    upload_url = upload_url_template.split('{')[0]

    files_to_upload = [
        'release/Duffer_Pro_v0.75-noonan.zip',
        'release/macos/Duffer_Pro_macOS_Sonoma.zip',
        'release/gba/duffer_pro.gba'
    ]

    for fpath in files_to_upload:
        if not os.path.exists(fpath):
            continue
        fname = os.path.basename(fpath)
        fsize = os.path.getsize(fpath)
        ctype = 'application/zip' if fname.endswith('.zip') else 'application/octet-stream'
        print(f"Uploading {fname} ({fsize} bytes)...")

        with open(fpath, 'rb') as f:
            data = f.read()

        up_headers = {
            'Authorization': f'token {token}',
            'User-Agent': 'Duffer-Release-Bot',
            'Content-Type': ctype,
            'Content-Length': str(fsize)
        }

        up_req = urllib.request.Request(
            f'{upload_url}?name={fname}',
            data=data,
            headers=up_headers,
            method='POST'
        )

        try:
            with urllib.request.urlopen(up_req) as up_resp:
                asset = json.loads(up_resp.read().decode('utf-8'))
                print(f"Uploaded: {asset['name']} -> {asset['browser_download_url']}")
        except Exception as e:
            print(f"Failed to upload {fname}: {e}")

if __name__ == '__main__':
    main()
