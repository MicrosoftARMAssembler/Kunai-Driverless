# Kunai-External

Kunai is **Valorant External** using a **1-Day Exploit** to **Read/Write Memory**. <br /> 
Driverless confirmed working on **Windows 10 and Windows 11**, including systems with **HVCI enabled**. <br />
You will need to **update** the **Valorant External** because it was updated a couple weeks ago. <br /> 

# How does the driverless work?
Kunai uses a **BYOVD (Bring Your Own Vulnerable Driver)** technique for temporary **Kernel Execution**. <br />
This is a **1-Day Exploit** because we use our **BYOVD** to exploit a **Patched Vulernablity** in windows. <br />
After exploiting the **Vulnerability** we map all **Physical Memory** and **Hide Our Process**. <br />

<details>
  <summary><b>BYOVD</b></summary>

  
Our **Vulnernable Driver** is **EBIoDispatch** and it's compliant with **Vulnerable Driver Blocklist**. <br />
When we unload our **Vulnernable Driver** we change **MmUnloadedDrivers** Cache to be from a **Critical Process**. <br />
Disgusting our **Vulnernable Driver** to be loaded from a **Critical Process** works because of our **Process Hiding**. <br />
</details>

<details>
  <summary><b>1-Day Exploit</b></summary>


Our **1-Day** uses the **BYOVD** to set our **KThread::PreviousMode** to **Kernel Mode**, <br />
then we use **NtOpenSection** to open a handle to **\\Device\\PhysicalMemory** to bypass **SP1 ( Windows Server 2003 Service Pack 1)** patch. <br />
The security patch **SP1** prevents **User Mode** processes from opening protected handles like **PhysicalMemory**.  <br />
After obtaining a handle to **PhysicalMemory** we restore our **PreviousMode** and map all **Physical Memory** in our process.  <br />

</details>

<details>
  <summary><b>Process Hiding</b></summary>


Before loading the **BYOVD** we use **Token Impersonation** to elvate our process to **NT User Authority**, <br />
and we enable **DACL Protection** to prevent handles being opened to our process and restart the process. <br />

Once we load the **BYOVD** and map all **Physical Memory** we unlink our **Handles** so they're not visible in **SystemInformation**, <br />
and unlink the **VAD Nodes** of all **Physical Memory** so they're not visible from **Kernel Mode**. <br />
After we finish cleanup we set our process as a **Process-Protected Light (PPL) Anti-Malware** process. <br />
</details>
<img width="1939" height="590" alt="image" src="https://github.com/user-attachments/assets/d3d09934-7db8-4569-8a5e-14771b6740a3" />

# Valorant External
The **Valorant External** has **ESP and Aimbot** that was **tested in unrated**. <br />
The Features it has is Box(Corner/Full), Health Bars, Agent and Weapon Names and uses **Discord Overlay**. <br />
If you get a report **Valorant** will take a **GPU Screenshot** and Discord does not block that. <br />

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/da5c0702-b59f-4550-8a10-146a7524a92c" />
<img width="3840" height="1080" alt="image" src="https://github.com/user-attachments/assets/13761682-9632-47e0-b2e9-6f018473aaf7" />

# Follow my Github and check out my other projects!
