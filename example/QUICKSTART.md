# 🚀 Quick Start - 5 Minute Setup

Get your DMA testing up and running in 5 minutes!

---

## ⚡ **Fast Track:**

```powershell
# 1. Open Developer Command Prompt for VS 2022 AS ADMINISTRATOR

# 2. Navigate to example folder
cd example

# 3. Build
.\build.bat

# 4. Run
cd x64\Release
.\test_dma.exe
```

---

## ✅ **Pre-Flight Checklist:**

Before running, make sure you have:

- [ ] FPGA DMA card installed in PCIe slot
- [ ] Target PC powered on and accessible
- [ ] Running as Administrator
- [ ] Visual Studio 2022 (or Build Tools) installed
- [ ] `vmm.dll` and `leechcore.dll` in `x64/Release/`

---

## 🎯 **First Test Sequence:**

Once the program starts:

1. **Select option 1** - Test initialization
   - Should see: `✓ DMA initialized with FPGA hardware!`

2. **Select option 2** - Find a process
   - Enter: `notepad.exe` (make sure it's running on target)
   - Should see: `✓ Process found! PID: XXXX`

3. **Select option 3** - Read memory
   - Enter address: `0x140000000`
   - Should see memory values displayed

4. **Select option 6** - Check metrics
   - Should see statistics of operations performed

---

## ❌ **Common Issues & Quick Fixes:**

| Problem | Solution |
|---------|----------|
| "Compiler not found" | Run from Developer Command Prompt |
| "VolkDMA.lib not found" | Build main library first: `cd .. && msbuild` |
| "DMA init failed" | Check FPGA connection, run as Admin |
| "DLL not found" | Copy vmm.dll & leechcore.dll to x64/Release/ |
| "Process not found" | Make sure process runs on target PC |

---

## 📞 **Need Help?**

Check the full README.md for detailed troubleshooting!
