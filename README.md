# py-freerdp
Basic Python wrapper for FreeRDP

Tested under Python 3.4.
> Currently seg faults on module exit. Need to debug.


```bash
sudo apt-get install -y python3.4-dev libfreerdp-client1.1 libfreerdp-gdi1.1
git clone https://github.com/tautek/py-freerdp
cd py-freerdp
git submodule update --init

# build locally
python3 setup.py build_ext --inplace

# install with pip
pip3 install --global-option=build_ext .
```

```python
from freerdp import FreeRDP
import sys

def connected(client):
    print("Connected.")

c = FreeRDP("/v:MYMACHINE /cert-ignore /u:MYUSER /p:MYPASS /d:MYDOMAIN", connected)
print("Press ENTER to exit")
sys.stdin.readline()
print("Goodbye.")
```

