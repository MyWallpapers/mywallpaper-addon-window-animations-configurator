# A07 — étude de sources, 2026-10-13

Preuve d’implémentation et d’étude, pas autorité de décision. Ledger lu en
premier : MyWallpaper `299548ebcc20770fd73b10bdffd5f05586fbe126`, empreinte
SHA-256 initial `ed477485a495b1e6ff9e931dfae5378461378c4c57977a4e436e753ca9ddba6c`.
Relecture avant livraison : SHA-256
`c52e81b2d36b773bd89538c86999d88098ba6d73b736e89b879a65029ea651fa`.

DEC-0012/0013 : comportements et contrats de surface dans l’add-on, garanties
génériques dans le core. Classification : conforme au mandat de développement.
DEC-0099/0160 : respecter les licences réelles, retirer Windhawk et distinguer
le moteur propriétaire de ses tiers et du SDK/CLI public MIT. Classification :
conforme, aucun code Windhawk/GPL ou code propre du moteur embarqué. Le snapshot
ancien a été étudié avant ce changement ; la licence courante du moteur n’est
pas présentée comme MIT. DEC-0157 : le développement local ne décide pas
d’un écosystème natif au lancement. Classification : conforme. DEC-0140 :
preuves de comportement et revue des entrées/callers au périmètre du changement.

## Référence MyWallpaper réellement étudiée

Taskbar Configurator `8e1fe2e6ddfa1acd51f9eb626f4f0dab09c21dc0` : README,
manifest.json, `ApplyPreset`/`ParseDeviceSettings` dans src/config.cpp,
`ConfigureMotion`, `BuildDirectProperties`, Start/SettingsChanged/Stop dans
src/taskbar_configurator.cpp, CMake et script de build. Codebase Memory a été
consulté avant les lectures (architecture, recherche des fonctions).

Le snapshot expose 45 définitions, dont six sections, 39 valeurs device,
conditions showIf et palettes ; ses couleurs/tailles personnalisées sont
appliquées après sélection de Custom. Les effets de pointeur sont des
animations Composition du XAML, avec sauvegarde/restauration des propriétés,
callbacks et handlers conservés jusqu’au drain. Nous reprenons les réglages
standards, profils, conditions d’affichage, snapshot complet et restauration,
sans transplanter les mécanismes XAML à des fenêtres d’applications.

Moteur `ff32c520dd4ccd9073c9dc001a180e886d50ab63` : README, MwHostApi/MwAddonApi,
CallGate, sdk/CMakeLists.txt et controller/src/process_v2.rs. L’ABI sait
résoudre des exports et gérer des hooks logiques résidents ; le descriptor
choisit une image et une classe optionnelle. Il n’expose pas un résolveur
universel de symboles privés UDWM ni un ciblage multi-processus automatique.
Le runtime réalisé choisit le mécanisme autorisé d’observation externe ; il
ne crée pas un second moteur d’injection et ne change pas le moteur commun.

## Mods de la famille et techniques

Catalogue épinglé à
`8d60b4c4113a7f0a18b4b48a5443a199acf54805`, famille A07. Les métadonnées de
tous les mods A07 ont été lues ; les chemins de capture/hooks/composition
pertinents ci-dessous ont été inspectés dans le checkout local et/ou la source
primaire. Les variantes alimentent une même bibliothèque de mouvements.

| Source | Mécanisme observé et apport | Frontière |
| --- | --- | --- |
| [MacOS Minimize Animation](https://github.com/ramensoftware/windhawk-mods/blob/8d60b4c4113a7f0a18b4b48a5443a199acf54805/mods/macos-minimize-animation.wh.cpp) | Hooks ShowWindow/Async, placement/position, capture PrintWindow, overlay alpha, moteur de déformation, ciblage taskbar UI Automation | Capture translucide et multi-écran spécifiques ; ne pas attribuer à ce mod un mécanisme de thumbnail DWM |
| [Genie Animation Mod](https://github.com/ramensoftware/windhawk-mods/blob/8d60b4c4113a7f0a18b4b48a5443a199acf54805/mods/genie-minimize-animation.wh.cpp) | Capture/cache, overlay, progression, origine apprise à partir du pointeur, transitions DWM temporaires | Résoudre une origine apprise ne prouve pas l’icône exacte ; préserver l’état original des attributs |
| [Windows Animations](https://github.com/ramensoftware/windhawk-mods/blob/8d60b4c4113a7f0a18b4b48a5443a199acf54805/mods/windows-animations.wh.cpp) | Chemins de capture/compatibilité par application, caches, observation et sous-classage | Ses nombreux correctifs ne deviennent pas une preuve de capture universelle pour notre backend GDI |
| [Disable Window Open/Close Animations](https://github.com/ramensoftware/windhawk-mods/blob/8d60b4c4113a7f0a18b4b48a5443a199acf54805/mods/disable-open-close-anims.wh.cpp) | Force-disabled DWM avant ShowWindow/SetWindowPos/DestroyWindow, rétablissement différé | Un événement postérieur ne suffit pas à supprimer sélectivement l’animation système |
| [Wobbly Windows](https://github.com/ramensoftware/windhawk-mods/blob/8d60b4c4113a7f0a18b4b48a5443a199acf54805/mods/wobbly-windows.wh.cpp) | Hooks de symboles privés udwm.dll, proxies/transforms, maillage et observation du déplacement/état | GPL-3.0-only ; non repris. Notre mode Wobbly anime une capture de transition, sans promesse de déformation vivante lors du déplacement |

Classic Minimize/Maximize Animations, Windows 7 Window Animations Restorer et
Mobile Open Animation motivent modes de zoom, durée, origine et ouverture.
Acrylic Color Glows concerne surtout un feedback de lumière translucide ; il
n’est pas réintroduit comme fond obligatoire dans une capture. Shell Animation
Disabler concerne des surfaces de shell appartenant à d’autres configurateurs
pour leur présentation ; aucune injection dans ces hosts n’est prétendue.

La déformation mathématique, le rasteriseur, les contrôles et le companion sont
des implémentations originales. Le header JSON est la seule source tierce
embarquée. Les attributs de composition ont été vérifiés dans
[Microsoft, DWMWINDOWATTRIBUTE](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute).
L’essai de cloak externe n’a pas fourni la lease attendue et ce setter a été
retiré. Le runtime conserve seulement la lecture de l’état cloaked pour éviter
d’interférer avec une fenêtre déjà masquée par Windows ; le masquage réalisé
utilise une opacité Win32 temporaire qualifiée sur la fixture.
Les captures et performances observées de notre runtime doivent être évaluées
à partir de la validation locale, pas de ces sources concurrentes.
