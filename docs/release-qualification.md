# A07 : qualification de la première publication

Preuve consommable, pas une décision parallèle. Ledger relu en premier :
SHA-256 `5c198b56bd404e3e035f31687e2ed23b7c0d87655e998bd0940fa2649ddae993`. Code audité : `ef4121c3b1fb80c40b1c7469273d4eec372cd6db` ;
MyWallpaper de référence : `299548ebcc20770fd73b10bdffd5f05586fbe126`.
Les changements suivants concernent uniquement cette preuve documentaire.
DEC-0012/0013/0140/0152/0160 : conforme au périmètre testé et aux frontières
constatées ; aucune modification du moteur, du SDK, du core ou du ledger.

## Résultats observés

- Contrat C++ Windows et SDK actuel : 69 définitions, 61 valeurs, six profils.
- Six mouvements × quatre transitions : 24 effets rendus dans le companion
  autonome puis 24 dans la vraie application MyWallpaper, restée ouverte et connectée.
- Désactivation et retrait du calque DEV : effets terminés, companion arrêté.
  Les trois calques persistants et leurs versions restent en place.
- 100 interruptions avec une lease d'opacité active, après 20 cycles de chauffe :
  style original restitué et aucune fenêtre overlay restante à chaque arrêt.
- Douze fenêtres bloquées : la désactivation observée dans le fichier de statut
  reste réactive ; le guardian n'est pas déclenché lors de ce contrôle.
- Fin stdin, conflit entre instances après init/progress, settings invalides,
  réduction des animations Windows, arrêt forcé et chunks malformés : contrôles passés.
- Les trois processus de l'add-on gardent environ 6 Mio privés en activité sur
  une fenêtre. Le nombre total de handles reste à 392 ; le cache retombe à zéro
  à la désactivation et tous les helpers disparaissent à shutdown.
- Deux builds frais, dans des clones et dossiers différents : les cinq fichiers
  du bundle sont identiques. Les deux exécutables correspondent aux octets testés
  dans MyWallpaper. MSVC 19.44, SDK Windows 10.0.26100, Windows 11 build 26200, 96 DPI.

Les compteurs, empreintes, échantillons et dates sont dans
`evidence/qualification-20261014.json`. Les résultats du futur build central
et l'identité du catalogue sont vérifiés séparément dans le rapport de livraison.

## Limites et incident conservé

Un premier run de la matrice sur les octets finals a déclenché le garde de
restauration de 1500 ms, interrompant un effet. La cause précise du retard de
la boucle de messages n'est pas démontrée. Ce résultat n'est pas effacé : la
matrice complète suivante, les 24 transitions via MyWallpaper et les 100
interruptions suivantes passent sans ce déclenchement. Le garde reste actif ;
aucun délai n'a été augmenté pour faire passer le contrôle.

Ce backend reçoit les événements après Windows et anime une image capturée,
avec un rendu CPU borné. Il peut manquer un effet ou le terminer sous charge ;
il ne garantit ni le remplacement de la transition système ni 60 fps.
Les applications accélérées/translucides, Mica, plusieurs écrans/DPI et une
journée d'exécution ne sont pas qualifiés. Le respect des animations réduites
reste activé par défaut ; les essais animés l'ont désactivé pour cet add-on
seulement, sans toucher à la préférence Windows.

Un build dans un dossier local très long a été refusé par les outils MSBuild.
La qualification utilise l'option existante BuildDirectory avec un chemin plus
court, sans modification de Windows. Aucun test GUI n'est ajouté au build central.
