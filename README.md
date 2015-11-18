# py-freerdp
Basic Python wrapper for FreeRDP

Tested under Python 3.4.

```bash
sudo apt-get install -y python3.4-dev libfreerdp-client1.1 libfreerdp-gdi1.1

python3 setup.py build_ext --inplace
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

