# Kunai-External

Kunai is **Valorant External** using a **1-Day Exploit** to **Read/Write Memory**. <br /> 
Driverless confirmed working on **Windows 10 and Windows 11**, including systems with **HVCI enabled**. <br />
You will need to **update** the **Valorant External** because it was updated a couple weeks ago. <br /> 

# How does the driverless work?
Kunai uses a **BYOVD (Bring Your Own Vulnerable Driver)** technique for temporary **Kernel Execution**. <br />
This is a **1-Day Exploit** because we use our **BYOVD** to exploit a **Patched Vulernablity** in windows. <br />
After exploiting the vulnerability we map all **Physical Memory** and unlink the **VAD**. (See [Process Protection](#process-protection)) <br />

<details>
  <summary><b>BYOVD</b></summary>

  
Our **Vulnernable Driver** is **EBIoDispatch** and it's compliant with **Vulnerable Driver Blocklist**. <br />
We unload our **Vulnernable Driver** and change **MmUnloadedDrivers** cache to be from a **Critical Process**. <br />
Disgusting our **Vulnernable Driver** to be loaded from a **Critical Process** works because of our **Process Protection**. <br />
</details>

<details>
  <summary><b>1-Day</b></summary>

</details>

<details>
  <summary><b>Process Protection</b></summary>

</details>
