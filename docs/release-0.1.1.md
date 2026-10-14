# A07 — livraison 0.1.1

Publié le 15 octobre 2026 dans le catalogue DEV utilisé par cette application. État serveur : available. [Source v0.1.1](https://github.com/MyWallpapers/mywallpaper-addon-window-animations-configurator/tree/v0.1.1) ; [reconstructions et attestations](https://github.com/MyWallpapers/native-addon-toolchain/actions/runs/38644212655).

## Corrections

- Captures réparties, une par tour de boucle : plusieurs fenêtres bloquées ne cumulent plus leurs délais sur un même tour ; sélection équitable conservée.
- Acquisition du mutex après init/progress : une seconde instance échoue proprement selon process-v2.
- Restauration du style corrigée et pixels du preview synchronisés avant capture.
- Build Visual Studio 2022/2026 reproductible. Le premier build central 0.1.0 échouait faute de Get-FileHash ; 0.1.1 utilise SHA-256 .NET, avec la même empreinte obligatoire.

## Qualification des binaires publiés

- Six modes × quatre transitions : 24 observations autonomes et 24 via le runtime de MyWallpaper ; mises à jour des paramètres, désactivation et retrait du calque passés. Les cinq fichiers testés correspondent exactement à l’index natif publié.
- Sidebar réelle : slider de vitesse 135 et ambiance Genie reçus par le contrôleur natif ; légendes visibles sans troncature à 398 px, aucun message d’erreur. Le preview livré est installé et arrêté par l’application.
- 100 interruptions avec fenêtre temporairement masquée : style original restitué et overlays retirés. Douze fenêtres bloquées : désactivation observée en 0.344 s. Arrêt brutal, fin stdin, conflit d’instances et données invalides passés ; aucun helper restant à l’arrêt.
- Mémoire privée agrégée du companion, de ses deux helpers et de conhost : 5.88 Mio au début du repos, 5.79 Mio à 30/60/90 s ; 392 handles constants. Cache nul à la désactivation (4.76 Mio privés). CPU agrégé sur 90 s : 0.97 seconde CPU. Somme des working sets : environ 36 Mio, avec pages partagées comptées plusieurs fois.
- MyWallpaper PID 51156 et Explorer PID 13472 restent ouverts et répondent ; session connectée, trois calques persistants conservés. Les calques DEV de test sont retirés.

## Périmètre et limites

Tests sur fenêtres Win32 opaques, Windows 11 build 26200, 96 DPI. L’intégration utilise le calque DEV autorisé de MyWallpaper avec les octets centraux publiés ; aucune activation persistante du nouveau calque publié. Mica, contenu accéléré, plusieurs écrans/DPI et une journée d’exécution restent à qualifier.

Un essai local antérieur a déclenché le garde de restauration de 1500 ms et interrompu un effet ; la cause du retard n’est pas établie. Les matrices et campagnes suivantes passent. Le garde reste actif. Les effets animent des captures après les événements Windows : remplacement complet des transitions et 60 fps ne sont pas garantis. Le respect des animations réduites est activé par défaut ; les essais l’ont désactivé uniquement pour cet add-on.

## Traçabilité

Version source aca42c1b7e2b1e3cb3ab3bbe7ff0ad5cf750a2f8 ; release 01a13c70-9ce7-7aa1-adf5-51d73b87f0a5 ; distribution sha256:4f087f54ac334372b91cde6b30e039f320d0d5f721fbd121d22dde584265cd1b. Companion SHA-256 8ef3ed77d5bc8873f401b8477791d505538390ef8f0327fa3ff69d5b3f908a6d. Core/moteur/SDK/ledger inchangés ; revue des références et entrées dynamiques dans docs/impact-review.md.

Preuve détaillée : evidence/delivered-20261015.json. Référence MyWallpaper 299548ebcc20770fd73b10bdffd5f05586fbe126 ; ledger SHA-256 5c198b56bd404e3e035f31687e2ed23b7c0d87655e998bd0940fa2649ddae993, relu avant audit et revérifié à la livraison. Ce rapport fournit des preuves ; il ne remplace pas le ledger.
