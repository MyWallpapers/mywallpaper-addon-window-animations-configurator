# Animations des fenêtres · MyWallpaper

Add-on natif A07 pour les réglages habituels de MyWallpaper. Le profil par défaut
**Fluide et expressif** suit le choix du propriétaire. Le manifeste contient
**69 définitions : 61 valeurs locales et huit sections**, sous la limite de 100
de DEC-0012. Aucun JSON, CSS ou sélecteur n’est nécessaire.

## Utiliser le configurateur

1. Construire le bundle Windows avec `native/build.ps1`, puis ajouter ce dépôt
   comme add-on de développement dans MyWallpaper.
2. Dans ses réglages, renseigner les noms des **Applications à animer**, un par
   ligne. Les exclusions ont priorité sur tous les profils.
3. Choisir une **Ambiance**, puis ajuster **Vitesse**, **Amplitude** et
   **Origine du mouvement**. Chaque transition dispose d’un interrupteur.
4. Activer **Afficher les fenêtres d’essai** pour manipuler une vraie fenêtre
   Win32 avec Ouvrir, Réduire, Restaurer et Fermer. Ce panneau teste les effets ;
   les réglages restent dans l’interface MyWallpaper.
5. Choisir **Personnalisée** pour modifier séparément le mouvement et la durée
   des quatre transitions, leur progression, zoom, opacité, rebond et souplesse.
6. Utiliser les trois **Profils particuliers** pour donner une ambiance et une
   vitesse propres à une application ou à ses boîtes de dialogue. Le dernier
   profil correspondant a priorité. « Aucune animation » exclut cette cible.

Le choix Windows de réduction des animations est respecté par défaut. Si
Windows les désactive, le configurateur ne joue aucun effet. Pour un essai
volontaire, son contrôle de confort peut désactiver ce respect uniquement pour
cet add-on ; il ne modifie jamais le réglage Windows.

Les profils ne remplacent pas les valeurs personnalisées enregistrées. Revenir
à **Personnalisée** les retrouve. La remise à zéro standard de MyWallpaper
renvoie le snapshot complet du manifeste. Un snapshot invalide conserve la
dernière configuration acceptée. Désactiver, changer les réglages ou arrêter le
companion termine les effets et rend les fenêtres momentanément masquées.

## Six ambiances

| Profil | Résultat |
| --- | --- |
| Fluide et expressif | Zoom modéré à l’ouverture/fermeture, glissement aux changements d’état |
| Discret | Fondu rapide de 150 ms à vitesse normale |
| Cinéma | Zoom ample, progression douce et Genie prolongé à la réduction/restauration |
| Ressort | Zoom, déplacement et rebond amorti |
| Genie | Déformation réelle de la capture vers une origine choisie |
| Wobbly, expérimental | Ondulation du maillage de la capture pendant les transitions |

Les snapshots complets se trouvent dans `presets/`. Ils sont validés contre le
SDK MyWallpaper actuel et contre le parseur natif. Les coins, l’ombre et le
contour uni/dégradé concernent **la capture animée uniquement**. Le contenu et
l’apparence normale de l’application restent les siens ; cet add-on n’est pas
le configurateur d’apparence des fenêtres A06.

## Rendu et limites concrètes

Le companion observe les fenêtres sélectionnées avec les événements Win32.
Il demande une capture PrintWindow dans un helper isolé, garde une capture récente en mémoire,
puis anime cette image dans un overlay alpha sans focus et sans interaction.
Genie et Wobbly déplacent les sommets d’un maillage ; ce sont des déformations
de pixels, pas simplement un changement de taille du rectangle.

L’ouverture et la restauration peuvent masquer brièvement la vraie fenêtre
par une opacité temporaire, en conservant les valeurs et styles d’origine.
Un guardian séparé vérifie le heartbeat du companion
et restaure ses leases après disparition du processus ou retard excessif.
Les leases vérifient le HWND, le PID, le thread et la date de création du
processus avant restauration. Le guardian ne modifie pas l’état de fenêtres
déjà masquées par Windows au début de l’effet. Il restaure seulement une opacité
encore détenue par l’add-on, sans écraser un changement ultérieur de l’application.
La capture dispose d’un délai de 120 ms ; un helper bloqué est abandonné. Aucun
GetWindowDC/BitBlt ne sert de repli susceptible de capturer une autre fenêtre.

L’observation est postérieure à l’événement : une première image peut être
visible avant l’ouverture animée, et l’animation système de réduction peut
encore se jouer. « Désactiver une transition » désactive la contribution de
cet add-on, sans promettre de supprimer la transition Windows correspondante.
La fermeture et la réduction utilisent la dernière capture : les fenêtres
très éphémères, les captures périmées ou les cibles dépassant les budgets sont
ignorées. Le point d’arrivée « barre des tâches » est son bord estimé sur
l’écran de la source, pas l’icône exacte de l’application.

La capture PrintWindow est destinée aux fenêtres Win32 opaques compatibles. Mica,
acrylique, transparence par pixel et applications accélérées restent à
qualifier. Elles sont ignorées par défaut lorsqu’elles sont identifiées comme
translucides ; l’option expérimentale ne garantit pas leur rendu. Une capture
vide laisse la vraie fenêtre disponible. Aucun fond artificiel ne remplace un
contenu manquant. Les processus système du shell, DWM et Codex sont exclus.
Wobbly ne déforme pas le glisser/redimensionner interactif ni la composition
vivante de DWM. Les bureaux virtuels sont hors A07.

Budgets : quatre effets simultanés maximum, 24 captures maximum, 64 Mio de
captures retenues, quatre millions de pixels par capture et huit millions par
frame. La durée utilise le temps écoulé, pas le nombre de frames. 30/60 sont
des débits visés ; les diagnostics donnent le coût mesuré du rendu. Les tailles
de la source sont en pixels physiques ; les distances et décorations sont
adaptées au DPI lors du démarrage d’un effet.

## Construire et vérifier

Windows 11 x64, checkout local Windows, Visual Studio C++ x64 2022 ou 2026
et son CMake compatible :

```powershell
powershell.exe -NoProfile -File native/build.ps1
powershell.exe -NoProfile -File tests/runtime.ps1
```

Le bundle est dans `native/out/windows-x86_64/` : `companion.exe`,
`window-animations-preview.exe`, licence et notices. Il utilise le contrat
companion **process-v2, wire version 5** : init, ready/progress, snapshots
complets de settings, messages et shutdown/EOF. Stdout contient seulement les
records encadrés. Les records et la file d’entrée de cet add-on sont bornés à
16 Mio et les chunks à un Mio ; les dépassements sont des erreurs observables.
Le snapshot maximal fourni par ces 61 valeurs est bien inférieur à cette
borne. Les paramètres et profils restent propriétaires de l’add-on.

Une seule instance de cet add-on est active par session Windows. Une seconde
couche reçoit un conflit explicite après le handshake init/progress, pour
éviter deux propriétaires d’opacité sans émettre de données avant init.

Les captures sont réparties entre les fenêtres : une seule demande susceptible
de bloquer par passage de la boucle de messages, y compris pour les ouvertures
simultanées. Une cible bloquée ne monopolise pas toutes les demandes. Le délai
de cache est un intervalle minimal par fenêtre ; sous charge, la capture peut
arriver plus tard et sa durée de validité reste vérifiée avant utilisation.

Test prolongé sur ses propres fenêtres Win32, avec Python et psutil :

```powershell
python tests/stress.py --bundle native/out/windows-x86_64 --report evidence/runtime/stress.json
```

Ce test bloque douze fenêtres d’essai, interrompt cent animations avec une
lease active et observe les ressources au repos pendant 90 secondes. Il est
séparé des contrats CTest, qui tournent aussi sans bureau interactif.

Ce companion d’observation externe ne charge aucun code dans les applications
ni dans `dwm.exe`. Le moteur natif courant et son SDK ont été étudiés ; son
companion d’injection à cible unique n’est pas substitué ou modifié. Le seul
code tiers embarqué est le header JSON MIT, vérifié par empreinte avant build.

Tests portables et contrat du manifeste depuis WSL :

```sh
cmake -S . -B .build/model -DCMAKE_BUILD_TYPE=Release
cmake --build .build/model -j 4
ctest --test-dir .build/model --output-on-failure
node tests/manifest-contract.mjs
```

Le dernier test lit le SDK construit du dépôt frère MyWallpaper ; définir
`MW_SCHEMA_ROOT` pour un autre emplacement. `tools/generate_manifest.py`
régénère le manifeste et les six profils.

## Preuves et suite de qualification

Voir `docs/validation.md`, `docs/source-study.md` et `docs/impact-review.md`.
`docs/shared-capability-contract.md` décrit précisément les primitives
manquantes pour remplacer les transitions Windows avant leur démarrage et
qualifier un éventuel mode de composition vivante. Aucune extension du moteur,
du core, du ledger ou de la portée de lancement n’a été effectuée ici.

Licence de ce dépôt : MIT. Les sources concurrentes ont été étudiées et non
copiées ; leurs notices et mécanismes sont distingués dans l’étude.
