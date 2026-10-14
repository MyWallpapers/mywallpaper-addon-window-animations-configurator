# A07 — impact et revue de code mort, 2026-10-13

Preuve d’implémentation, suivant l’auditProtocol du ledger lu en premier.
MyWallpaper examiné : `299548ebcc20770fd73b10bdffd5f05586fbe126` ; ledger
SHA-256 `c52e81b2d36b773bd89538c86999d88098ba6d73b736e89b879a65029ea651fa`.
Ce dépôt est nouveau, sur `codex/window-animations`, sans commit antérieur.
L’inventaire source de `evidence/build-validation.json` identifie l’état livré.
Cette revue ne modifie aucune décision.

## Périmètre et décisions concernées

Seul `mywallpaper-addon-window-animations-configurator` est modifié. Le core,
le moteur, le configurateur Taskbar et le ledger sont des références en lecture.
Le ledger a changé pendant la mission ; les mentions de la licence du moteur
ont été corrigées d’après sa relecture, sans écrire dans ces références.

| DEC | Preuve locale | Classification |
| --- | --- | --- |
| DEC-0012 | 69 définitions, 61 valeurs device, huit sections ; SDK actuel accepte les six snapshots | Conforme au contrat de surface |
| DEC-0013 | Profils, filtres, rendu, paramètres et overlays dans cet add-on ; aucun changement du service commun | Conforme au mandat local |
| DEC-0099/0160 | MIT explicite pour le code original ; seul nlohmann MIT embarqué ; aucun code Windhawk ni code propre du moteur propriétaire | Conforme aux frontières constatées |
| DEC-0140 | Contrats C++/SDK, observations Windows et références/callers ci-dessous | Preuve au périmètre livré |
| DEC-0157 | Développement local et besoins techniques proposés ; aucune ratification d’un écosystème natif au lancement | Conforme ; qualification de lancement distincte |

## Références et chemins de consommation

Codebase Memory a été interrogé avant exploration. L’index final local contient
900 nœuds et 1442 arêtes, sans persistance dans le dépôt. Les traces à profondeur
deux ont été rapprochées des appels exacts et des registrations dans les sources ;
le graphe seul ne suffit pas à prouver l’absence d’usage d’un callback Windows.

| Surface touchée | Entrées et consommateurs vérifiés | Revue d’obsolescence |
| --- | --- | --- |
| `src/config.cpp`, `include/animation.hpp` | `ParseSettings` depuis init/settings et tests ; `ResolveMotion` depuis `Runtime::OnEvent` ; `Color`/`MatchesKind` depuis rendu/filtres | IDs du manifeste et snapshots générés conservés cohérents ; pas d’ancien contrat publié à migrer |
| `src/motion.cpp`, `src/render.cpp` | `Render` dans OnEvent/Tick et tests ; `Warp`, `Ease`, `FrameOpacity` dans le rasteriseur | Six modes réellement consommés ; bornes de zoom/ressort testées aux amplitudes extrêmes ; aucun mode gardé comme branche vide |
| `src/runtime.cpp`, `include/runtime.hpp` | WndProc/WinEvent/TIMER dispatchent OnEvent/Tick ; Update, Stop, SystemChanged et le message cancel passent par CancelAll | `lastEvent` inutilisé supprimé ; pending/caches sont vidés au changement de snapshot et à l’arrêt |
| `src/capture.cpp`, `include/capture.hpp` | CaptureClient depuis Start/OnEvent/Tick ; `--capture` dispatché par wmain ; helpers lancés depuis leurs propriétaires | Essai direct de WM_PRINT avec DC interprocessus et repli GetWindowDC/BitBlt retirés après capture incorrecte ; aucun appel résiduel |
| `src/watchdog.cpp` | Hide/Restore/Beat depuis runtime ; GuardMain depuis `--guard` ; RestoreLease depuis destructor, Hide, Restore et boucle guardian | Setter DWMWA_CLOAK et ancien Cloak retirés ; lecture DWMWA_CLOAKED conservée pour protéger les fenêtres déjà masquées par Windows |
| `src/companion.cpp` | wmain est le point d’entrée CMake ; Reader/WndProc sont enregistrés dynamiquement ; contrat externe process-v2 v5 | Aucun export SDK d’injection ou DLL hook dormant ; plafond des records/chunks et erreur d’ordre consommés par le pilote Windows |
| `src/preview.cpp` | wWinMain et deux WndProc enregistrés ; commandes natives et WM_PRINT/WM_PRINTCLIENT ; RefreshButtons depuis création/état de canvas | Les handlers d’impression sont nécessaires à PrintWindow et conservés malgré leur absence d’appel direct ; UpdateWindow garantit un contenu peint |
| `tools/`, `tests/`, presets et bundle | Générateur de manifeste, script d’export des preuves, CMake/CTest, script runtime, commande de build du configurateur | Outils référencés par README/validation ; binaires produits et déclarés dans mywallpaper.config.json ; sorties temporaires ignorées |

La trace entrante de ResolveMotion retrouve OnEvent et ConfigContract ; celle
de CancelAll retrouve Update, Stop, SystemChanged et Tick. RestoreLease a pour
callers le destructor, Hide et Restore, plus l’entrée native du guardian.
CaptureClient::Request est appelé via son objet depuis runtime ; cette relation
n’est pas complètement résolue dans le graphe et a été vérifiée dans les sources.

## Suppressions et chemins conservés

Le masquage n’utilise plus DWMWA_CLOAK. Le contrôle prépublication
`cloakEntering` a été remplacé par `hideEntering` dans le parseur, le manifeste
et les six snapshots. Ce dépôt n’avait pas de version publiée ; aucune donnée
externe persistée n’est supposée migrée.

La restauration des anciennes valeurs alpha/key des fenêtres déjà layered a
été retirée seulement après vérification que Hide refuse ces fenêtres à deux
points avant mutation. SetLayeredWindowAttributes ne doit pas changer leur
contrat UpdateLayeredWindow. Le chemin restant retire uniquement WS_EX_LAYERED
ajouté par l’add-on, vérifie l’identité HWND/PID/thread/création et conserve les
bits ou opacités changés ultérieurement par l’application. La protection contre
un setter de masquage tardif restaure aussi une lease déjà retirée par le garde.

PrintWindow demeure synchrone dans le helper isolé. Seul ce helper créé et
détenu par le companion est terminé après dépassement ; aucune application
cible n’est terminée. Les callbacks d’événement ne font qu’un PostMessage borné.
Les entrypoints guard/capture, handlers natifs, messages process-v2 et chemins
de build sont des contrats dynamiques vérifiés, donc conservés.

Aucune suppression ne repose sur la seule non-observation au runtime. Le
header tiers n’a pas été élagué. Aucun workspace partagé, export public du
core ou contrat du moteur n’a été supprimé ou modifié.

Les fins de ligne du dépôt sont déclarées dans .gitattributes. Les octets tiers
ne sont pas normalisés ; l’espace original du titre de leur licence est conservé.
Les lignes vides redondantes en fin de fichiers propres ont été retirées.

## Finalisation A07, 2026-10-14

Ledger relu avant cette passe, SHA-256
`5c198b56bd404e3e035f31687e2ed23b7c0d87655e998bd0940fa2649ddae993` ;
MyWallpaper de référence toujours `299548ebcc20770fd73b10bdffd5f05586fbe126`.
Classification DEC-0012/0013/0140 : corrections locales et preuves de comportement,
sans changement de responsabilité, de contrat public ou de décision du ledger.

- `Runtime::Tick`, appelé par `WindowProc/WM_TIMER`, traite au plus une ouverture
  différée ou une capture périodique par tour. `Window::attempted` est consommé
  par la sélection du plus ancien essai, même après échec. Les bornes de mémoire,
  la validité de capture, les phases et le nettoyage Update/Stop sont conservés.
  La boucle antérieure de captures groupées est remplacée ; aucun chemin de
  capture parallèle, registration WinEvent ou entrée du helper n'est supprimé.
- `companion.cpp` acquiert le mutex de session dans le chemin init après progress.
  Son ancien emplacement dans wmain est retiré après vérification du handshake,
  du conflit entre instances et de la fermeture du handle à la fin de wmain.
  Reader, callbacks et modes `--capture`/`--guard` restent des entrées dynamiques.
- La correction antérieure du setter de style dans `Watchdog::Hide` vérifie
  simultanément la valeur de retour et LastError. `RestoreLease` et toutes ses
  entrées sont conservés. `preview.cpp/WM_PRINT` conserve GdiFlush pour livrer
  les pixels au helper avant son retour.
- `native/build.ps1` sélectionne le générateur correspondant à Visual Studio
  2022/2026 et fixe l'instance détectée. Le script reste appelé par le contrat
  de publication `mywallpaper.config.json` et par le build local documenté.
  Les deux exécutables gardent leurs noms et leurs chemins. `/Brepro` couvre
  maintenant aussi le preview et les deux liens ; les sorties déclarées sont
  inchangées. Aucun champ local de configuration absent du build central ajouté.
- `tests/runtime.ps1` exerce le nouveau handshake avant le conflit ;
  `tests/stress.py`, explicitement documenté, exerce des leases réelles,
  douze cibles bloquées et l'arrêt des helpers. Il n'est pas enregistré dans
  CTest : ce test a un contrat externe de bureau Windows interactif.

Pas de suppression fondée sur la seule non-observation. Aucun changement du
core, du moteur partagé, du SDK ou du ledger. La qualification courante et
l'identité exacte de publication sont consignées dans le rapport de livraison.
