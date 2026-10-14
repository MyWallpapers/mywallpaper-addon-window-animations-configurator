# Besoins techniques communs — contrat minimal à qualifier

Ce document précise des besoins de cette expérience ; il ne ratifie pas une
architecture et ne modifie aucun contrat partagé. Référence : ledger courant,
DEC-0012/0013/0157, commit MyWallpaper
`299548ebcc20770fd73b10bdffd5f05586fbe126`, SHA-256 ledger
`c52e81b2d36b773bd89538c86999d88098ba6d73b736e89b879a65029ea651fa`.

## 1. Cible de processus exacte pour une future voie à hooks

Le descriptor actuel choisit imageName/windowClass ; plusieurs instances de la
même image ne constituent pas une sélection explicite de processus.

Primitive minimale proposée :

```json
{"processIdentity":{"pid":1234,"creationTime100ns":"134000000000000000"}}
```

Ce choix est exclusif du ciblage par image ; vérifier le couple PID/date avant
ouverture, avant injection et avant nettoyage. Retourner une erreur explicite
pour disparition, réutilisation du PID, architecture non supportée ou accès
refusé. Une cible exacte n’autorise aucune injection système globale.

Preuve d’usage locale précise : `Identify`/`Same` et `RestoreLease` conservent
déjà PID, thread et création pour les cibles d’overlays et la restauration.
`tests/runtime.ps1` force la disparition du companion avec une lease active,
puis vérifie la fenêtre restaurée. Il faut réutiliser cette identité pour une
future orchestration des hooks dans les processus explicitement sélectionnés.
Cette orchestration demeure à implémenter/qualifier ; le backend externe suit
déjà plusieurs fenêtres et nouveaux processus sans injection.

## 2. Observer l’intention avant la transition

Pour supprimer l’animation système exactement à l’ouverture/fermeture,
le handler doit agir avant ShowWindow/placement/DestroyWindow et préserver
l’état d’origine de la fenêtre. L’engine expose déjà resolve_export et les
opérations de hooks ; les handlers métier, filtres et profils restent ici.

Échange minimal entre un futur hook add-on et son renderer :

```text
capture-request {windowIdentity, generation, previousPhysicalRect, event}
capture-result  {generation, immutableFrameId | unavailable}
transition-completed {generation, succeeded, actualPhysicalRect}
cancel {generation}
```

La génération interdit les retours tardifs. Le hook n’attend jamais un RPC
controller/renderer non borné. Si aucune capture récente n’est disponible,
laisser Windows agir. Toute restauration vérifie l’identité et restaure
l’attribut original, sans écrire une valeur supposée par défaut. Le rendu
survit à la destruction de la fenêtre et à la sortie de son application.

Preuve d’usage locale : `ResolveMotion`, `Render` et les quatre `Event` sont
les mêmes points de consommation, testés pour les six modes et les deux sens.
Les départs fermeture/réduction utilisent déjà une image immuable ; un nouveau
transport ne doit pas réinventer les modes ou paramètres dans le moteur.
Les fixtures actuelles prouvent la consommation et le rendu, pas la future
interception avant transition ni sa cohabitation avec d’autres hooks.

## 3. Capture accélérée et déformation de composition vivante

Le mode externe PrintWindow isolé reste utilisable indépendamment. Pour une meilleure capture,
un fournisseur peut livrer un frame immuable avec taille physique, région de
contenu, format BGRA premultiplié, horodatage, identité et coût observable,
sans définir d’animations métier. Il doit signaler refus de capture et contenu
indisponible. Conformité à tester : alpha, DPI, fenêtres couvertes, contenu
protégé, redimensionnement, écran retiré, coût et annulation.

Une vraie déformation vivante Wobbly dans DWM nécessite une qualification
distincte des symboles privés, versions de Windows et licences. Le résolveur
d’exports courant ne la promet pas. Ni source GPL du mod, ni API privée UDWM,
ni changement du moteur n’est intégré à ce dépôt.
