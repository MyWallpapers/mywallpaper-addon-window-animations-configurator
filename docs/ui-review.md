# A07 — revue de finition de la surface, 2026-10-13

Cette preuve suit l’auditProtocol du ledger courant, DEC-0012/0013/0140/0157/0160.
Commit MyWallpaper examiné : `299548ebcc20770fd73b10bdffd5f05586fbe126` ; ledger
SHA-256 `c52e81b2d36b773bd89538c86999d88098ba6d73b736e89b879a65029ea651fa`.
Elle ne ratifie ni une architecture ni le lancement. État source identifié par
`evidence/build-validation.json`.

Reviewer indépendant : sous-agent `impeccable_finish_reviewer`, lecture seule.
Périmètre : brief de surface, PRODUCT, manifeste, README, fenêtre Win32 d’essai
et quatre framebuffers. La sidebar MyWallpaper n’a pas été rendue pendant cette
mission. Le détecteur de design a été exécuté une fois sur manifest/preview et
n’a retourné aucun finding ; cela ne valide pas un rendu du manifeste.

## Contrat de présentation

Le parcours commence par activer, choisir une ambiance, régler vitesse/amplitude,
sélectionner les applications et les quatre transitions. Personnalisation,
apparence de la capture, profils particuliers et ressources sont conditionnels
ou repliés. « Fluide et expressif » suit le choix explicite du propriétaire.
Classification DEC-0012/0013 : conforme au brief et au contrat du manifeste.

## Findings résolus

1. Le helper Origine ne citait que Genie. Il explique maintenant la direction du
   glissement/ressort et la cible du zoom pointeur/barre, avec un bord de barre
   estimé et aucune promesse de viser l’icône exacte. Manifeste régénéré ; contrat
   SDK et parseur natif passés. Classification : écart d’implémentation corrigé.
2. Les PNG conservaient des canaux RGB prémultipliés, trop sombres dans un viewer
   PNG ordinaire. L’exporteur vérifie ce format, déprémultiplie les canaux avec
   arrondi et conserve l’alpha. Source BMP et sortie PNG portent leurs SHA-256.
   Les quatre images sont lisibles ; aucune nouvelle capture requise pour ce
   défaut d’encodage. Classification : preuve de présentation corrigée.

Le second passage du reviewer a rendu **ship pour ces corrections**, avec hashes
des sources/sorties vérifiés et les quatre images cohérentes avec leurs noms.
Le documenter relève ensuite le système observé dans DESIGN.md et le sidecar,
sans inventer les tokens de la sidebar héritée.

## Limites de ce verdict

Les images montrent des frames natives à Windows 11 build 26200, 96 DPI.
Elles montrent les pixels et déformations calculés, sans démontrer à elles seules
la fluidité temporelle complète. Elles ne montrent pas le contrôleur d’essai
séparé ni les contrôles MyWallpaper. Rendu, responsive, navigation clavier et
focus de la sidebar restent à observer dans le vrai host. La disposition ne
vaut pas approbation de lancement.
