# A07 — validation locale, 2026-10-13

Preuve d’implémentation selon l’auditProtocol du ledger, lu avant l’étude.
MyWallpaper examiné : `299548ebcc20770fd73b10bdffd5f05586fbe126` ; ledger
SHA-256 `c52e81b2d36b773bd89538c86999d88098ba6d73b736e89b879a65029ea651fa`.
Le dépôt d’add-on est nouveau, sans commit de référence. L’inventaire source et
les SHA-256 des binaires livrés sont dans `evidence/build-validation.json`.
Ce rapport n’étend ni le contrat partagé ni la portée de lancement.

## Résultat

Le bundle Release x64 a été compilé et testé sous Windows. Le modèle portable a
été compilé sous WSL. Les contrats C++ ont passé sur les deux plateformes ; le
SDK public actuel accepte **69 définitions, 61 valeurs device, huit sections et
six snapshots complets**. Classification DEC-0012/0013 : conforme au contrat de
surface et à la responsabilité de l’add-on.

Les tests couvrent les defaults du manifeste, options et bornes de sliders,
valeurs rejetées, règles par application/type de fenêtre, priorité du dernier
profil correspondant, désactivation par transition et facteur de vitesse.
Ils couvrent aussi les extrêmes d’amplitude/zoom/ressort, les coordonnées
négatives, les extrémités et la monotonie des easings, le maillage Genie, les
six mouvements dans les deux sens et les pixels alpha prémultipliés. Une source
semi-transparente vérifie qu’elle n’est pas prémultipliée deux fois.

Le build Windows utilise Visual Studio C++ x64, SDK Windows 10.0.26100 et CMake,
CRT statique. Les warnings MSBuild MSB8029 (dossier de build temporaire) et
MSB8064 (casse du chemin CMakeLists sur le checkout WSL) sont consignés ; la
compilation et CTest se terminent avec succès. L’empreinte du header nlohmann
est contrôlée avant build. Classification DEC-0099/0160 : seul ce tiers MIT
est embarqué, sans Windhawk ni code propre du moteur propriétaire.

## Observations Windows sur le bundle final

Pilote : `tests/runtime.ps1`, exécuté avec **Windows PowerShell 5.1**.
Environnement : Windows 11 Professionnel, version 10.0.26200, build 26200,
96 DPI. Cible : notre propre fenêtre Win32 dans window-animations-preview.exe.
Le pilote adresse uniquement ses processus et fenêtres d’essai.

Pour chaque mode, les quatre transitions ouverture, réduction, restauration
et fermeture sont observées avec une durée demandée de 600 ms : **24 transitions**.
Le compteur de frames progresse, les effets terminent et les images d’ouverture
contiennent le client bleu attendu, pas seulement un titre ou une capture vide.

| Mode | Ouverture / réduction / restauration / fermeture | Frames émises sur les quatre effets | Dernier lastFrameMs |
| --- | --- | ---: | ---: |
| Fondu | 1 / 1 / 1 / 1 | 79 | 14,8126 |
| Glissement | 1 / 1 / 1 / 1 | 74 | 12,8715 |
| Zoom | 1 / 1 / 1 / 1 | 79 | 7,7577 |
| Ressort | 1 / 1 / 1 / 1 | 77 | 8,1572 |
| Genie | 1 / 1 / 1 / 1 | 84 | 0,9311 |
| Wobbly | 1 / 1 / 1 / 1 | 82 | 8,4700 |

lastFrameMs est un dernier échantillon de coût, pas une moyenne, un percentile
ou une garantie de débit. 60 fps est la consigne ; ce test n’établit pas que
chaque effet atteint 60 fps, ni que le coût se généralise à d’autres fenêtres.

Les assertions de lifecycle et de protection ont passé :

- Un snapshot invalide garde la dernière configuration acceptée ; changement
  de profil et remise à zéro complète retrouvent Fluide.
- Désactivation globale et désactivation de la seule réduction suppriment la
  contribution demandée de l’add-on.
- Le thread UI de la cible est bloqué volontairement pendant 1,8 seconde ; le
  helper de capture est abandonné et les settings du companion restent réactifs.
- La préférence **réelle** de Windows demandait des animations réduites ;
  respectReducedMotion=true a supprimé les effets. Les essais de mouvement ont
  utilisé false pour cet add-on seulement, sans changer le réglage Windows.
- Arrêt forcé du companion pendant une lease d’opacité active : le guardian
  séparé restaure la fenêtre d’essai avant le délai de vérification de trois secondes.
- EOF stdin termine proprement ; une seconde instance reçoit addon_owner_conflict.
- Header tronqué et chunk hors ordre sont rejetés avec invalid_wire et exit 1.

Les six observations de protection et les compteurs sont copiés dans
`evidence/windows-26200-results.json`. Les logs et buffers bruts du run
`evidence/runtime/windows-26200-delivered-bytes/` restent locaux et ignorés par Git.
Une invocation du pilote depuis PowerShell 7 a bloqué sa lecture de sortie et
a été interrompue ; elle n’a produit aucun résultat qualifié. Le runner documenté
et utilisé pour les résultats ci-dessus est Windows PowerShell 5.1.

## Images et revue

Les quatre PNG dans `evidence/native-frames/` proviennent du run Windows
`windows-26200-qualified`, à 96 DPI, sur la même fenêtre d’essai. Ce sont des
sorties du rasteriseur, avec une frame retenue à partir d’une progression de 0,38,
et non des screenshots de la sidebar. `tools/export_frames.py` encode correctement
leur alpha droit à partir des BGRA prémultipliés ; chaque sidecar JSON identifie
le BMP source et les empreintes source/PNG. L’arrondi d’export est documenté.
Aucun de ces rasters ne fait partie du bundle runtime.

La revue indépendante de finition et ses corrections sont dans
`docs/ui-review.md` ; le système observé est décrit dans DESIGN.md et son sidecar.
Classification DEC-0140 : preuve de comportement au périmètre exercé.

## Qualification encore requise

Les fixtures ne qualifient pas Mica/acrylique, alpha par pixel, applications
accélérées, contenu protégé, fortes charges, DPI autres que 96, écrans
secondaires ou changements de topologie. La conversion des paramètres selon le
DPI existe dans les sources, mais une qualification multi-écran reste nécessaire.
Rendu et navigation des contrôles dans la vraie sidebar MyWallpaper n’ont pas
été observés pendant cette mission. Le contrat SDK constitue une preuve distincte.

Le backend externe reçoit des événements après la transition. Il ne démontre
pas un remplacement intégral des animations système, une destination à l’icône
exacte de la barre ou une déformation Wobbly vivante lors du déplacement.
Les besoins précis d’une voie avant transition/capture accélérée sont dans
`docs/shared-capability-contract.md`, sans modification du moteur commun.
Les bureaux virtuels restent hors A07. Classification DEC-0157 : développement
local conforme ; aucune décision de lancement n’est prise par cette validation.
