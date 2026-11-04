# PT100 SDK v6.3.2-20260630

芯片：CV1842CP；板型：pt100_spinand；工具链：ARM musl。

## 获取与复现

将本目录作为 `pt100_bsp` 放到 SDK 根目录，并在 SDK 根目录执行：

```bash
./pt100_bsp/scripts/repos --gitclone --reproduce
./pt100_bsp/scripts/repos --applypatch
./pt100_bsp/scripts/sync.sh
```

`manifest/repo_config` 指向 GitHub BSP 的固定版本快照。`scripts/repos` 是完整脚本文件，按快照复现后应用 `patches/` 与 `patches_public/`，并同步本目录的板级配置。

## 编译

```bash
SDK_ROOT=$(pwd)
export TPU_REL=1
source "$SDK_ROOT/build/envsetup_soc.sh"
defconfig cv1842cp_pt100_spinand
build_all
```

完整 upgrade 包位于 SDK 构建产物目录；请使用适配实际硬件的烧录流程。
