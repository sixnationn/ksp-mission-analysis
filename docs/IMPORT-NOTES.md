# Verified upstream configuration findings

Read-only source research on 22 September 2026. These are upstream release findings, not verification of the supplied game archive.

| Configuration | Meaning |
|---|---|
| JNSQ 0.10.2 standard | Kerbin radius 1,600,000 m; rotation period 43,200 s; approximately 2.7 times stock scale |
| JNSQ optional Rescale_10X | Kerbin radius and semi-major axis multiplied by 4 relative to JNSQ; radius 6,400,000 m; rotation period multiplied by 2 to 86,400 s |
| Reborn v1.0.1 | Replaces body nodes with internal names such as JNSQKerbin and JNSQMinmus, retaining familiar display names |
| Reborn SystemScale | Standard default or Real; Real applies its own per-body scaling. Do not double-apply JNSQ optional 10X |

Sources: [JNSQ Kerbin](https://github.com/Galileo88/JNSQ/blob/0.10.2/GameData/JNSQ/JNSQ_Bodies/Kerbin.cfg), [10X Kerbin](https://github.com/Galileo88/JNSQ/blob/0.10.2/Optional%20Mods/JNSQ_Rescale/Rescale_10X/GameData/JNSQ_Rescale/JNSQ_Bodies/Kerbin.cfg), [Reborn configuration](https://github.com/rbeap/JNSQ-Reborn/blob/v1.0.1/GameData/JNSQ-Reborn/JNSQReborn-Configuration.cfg), [body replacement settings](https://github.com/rbeap/JNSQ-Reborn/blob/v1.0.1/GameData/JNSQ-Reborn/Configs/KopernicusSettings.cfg), [Reborn Kerbin rescale](https://github.com/rbeap/JNSQ-Reborn/blob/v1.0.1/GameData/JNSQ-Reborn/Bodies/Rescale/Kerbin.cfg).

JNSQ Minmus semi-major axis is 146,970,000 m without Principia and 58,550,000 m with it; tidal-locking settings also change. Under JNSQ optional 10X the selected axis is multiplied by 4, giving 587,880,000 m versus 234,200,000 m. This is a concrete fixture for conditional patch evaluation. Reborn Mun also has a Principia branch. [JNSQ Minmus](https://github.com/Galileo88/JNSQ/blob/0.10.2/GameData/JNSQ/JNSQ_Bodies/Minmus.cfg), [Reborn Mun](https://github.com/rbeap/JNSQ-Reborn/blob/v1.0.1/GameData/JNSQ-Reborn/Bodies/Mun.cfg).

ModuleManager ordering includes FIRST, unordered patches, mod BEFORE/FOR/AFTER passes, LAST and FINAL. Presence conditions and deletion/replacement matter. Raw config concatenation cannot recover the loaded system. [Patch ordering](https://github.com/sarbian/ModuleManager/wiki/Patch-Ordering).

Reborn's README requires JNSQ and its dependencies. The external analysis tool need not load visual mods, but the runtime comparison installation must satisfy whatever dependencies actually affect loading and system state. Audit this distinction before omitting packages. [Reborn release source](https://github.com/rbeap/JNSQ-Reborn/tree/v1.0.1).

JNSQ Kronometer configuration uses home-day/home-year settings. Reborn RealTime=True defines a 365-day Jan-Dec display with offsetYear=2001, offsetDay=1 and offsetTime=21600; Real scale changes offsetTime to 43200. RealTime=False uses home day/year. Kronometer defaults useLeapYears to false unless another patch changes it. Inspect the resolved value, not just the default. Kronometer format tokens <Y0>/<D0> are unit symbols, not instructions for numeric zero-based dates. Implement the requested Y0,D0 display explicitly. [JNSQ Kronometer](https://github.com/Galileo88/JNSQ/blob/0.10.2/GameData/JNSQ/JNSQ_Configs/Kronometer.cfg), [Reborn configuration](https://github.com/rbeap/JNSQ-Reborn/blob/v1.0.1/GameData/JNSQ-Reborn/JNSQReborn-Configuration.cfg), [Kronometer syntax](https://github.com/Kopernicus/Kronometer#kronometer-syntax).

Acceptance matrix: JNSQ standard and optional 10X, each with Principia on/off; Reborn Standard and Real, each with Principia on/off. Check stable IDs, body count, parent relationships, radius, gravitational parameter, orbit, rotation, epoch, conditional Minmus/Mun states and calendar boundaries. Start with the user's selected Reborn Real fixture, then broaden after the adapter works.
