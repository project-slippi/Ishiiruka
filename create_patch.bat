git diff slippi feature/online-support > online.patch

rm -rf OnlinePatch
git format-patch -o "OnlinePatch" --full-index origin/slippi..feature/online-support
rm -rf online.zip
powershell Compress-Archive OnlinePatch online.zip