# m33rpfwu (FWU M33 RPMSG)

`m33rpfwu` is a Linux command-line tool for communicating with an M33 coprocessor over RPMsg to perform firmware update operations. This utility is part of the firmware update process for an STM32MP2x platform with the **M33TD flavor**, where writing and installing M33 binaries must be delegated to the coprocessor, since only it has access to the memory regions associated with these binaries. This code is provided as an example and can be tested with an OpenSTLinux version **6.2.1 or later**.

## Features

- RPMsg commands:
  - `list`
  - `info`
  - `cancel`
  - `install`
  - `reboot`
  - `accept`
  - `reject`
- Local command:
  - `write` to copy a binary into UIO shared memory

## RPMsg endpoint

- Endpoint name: `fwu`
- Control device: `/dev/rpmsg_ctrl0`

If the endpoint does not exist, the tool tries to create it with:

```bash
rpmsg_create_ept /dev/rpmsg_ctrl0 fwu 0x5B
```

## External dependency

`rpmsg_create_ept` is required and is provided by [**OpenAMP**](https://github.com/OpenAMP/open-amp)

## Build

The program can be cross-compiled using the **ST SDK** (see [Getting_started](https://wiki.st.com/stm32mpu/wiki/Getting_started)) or built with a native Linux toolchain.

```bash
make
```

The binairy named `m33rpfwu` can then be added in yout `/bin` folder.

## UIO Shared memory
The FWU process need a shared memory between A35 and M33 coprocessor to exchange binaires. On linux UIO device are used for this purpose.
- The tool expects a [UIO device](https://www.kernel.org/doc/html/v6.18/driver-api/uio-howto.html) named `uio-fwu-shmem`.
- Example UIO device definition in the device tree:

```dts
uio_fwu_shmem: uio-fwu-shmem@81300000 {
    compatible = "uio-fwu-shmem";
    reg = <0x0 0x81300000 0x0 0x00a00000>;
    status = "okay";
};
```

|    Name    | Id  |  UIO Offset  |
|------------|-----|--------------|
| `tfm_s_ns` | `0` | `0x00000000` |
| `ddr_fw`   | `1` | `0x00900000` |


## Usage

Basic update of ddr firmware:
```bash
m33rpfwu info
m33rpfwu info -c tfm_s_ns
m33rpfwu write -c m33fw -b /home/root/download/tfm_s_ns.bin
m33rpfwu install -c m33fw
m33rpfwu reboot
m33rpfwu accept
```

Type ```m33rpfwu help``` for more commands and infos.
