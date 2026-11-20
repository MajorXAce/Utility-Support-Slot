"""Deterministic C-only player archive. No install, launch or publication."""
from pathlib import Path
import io,zipfile
PLAYER_FILES=('hellforge-mod.json','icon.png','LICENSE','THIRD_PARTY_LICENSES.txt','README.md','utility_support_slot.dll')
def archive_bytes(root,dll):
    root=Path(root);data=io.BytesIO()
    with zipfile.ZipFile(data,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as package:
        for name in PLAYER_FILES:
            path=Path(dll) if name=='utility_support_slot.dll' else root/name
            info=zipfile.ZipInfo(name,(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
            package.writestr(info,path.read_bytes())
    return data.getvalue()
