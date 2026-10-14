"""Generate the native-only settings contract; sections count toward the 100 limit."""
import json
from pathlib import Path

root = Path(__file__).resolve().parent.parent
settings = []
group = None

def section(key, label, collapsed=False):
    global group
    group = key
    settings.append(dict(id=key, type="section", label=label, defaultCollapsed=collapsed))

def setting(key, kind, label, default, description="", show=None, **extra):
    value = dict(id=key, type=kind, label=label, default=default, scope="device", **extra)
    if group:
        value["parent"] = group
    if description:
        value["description"] = description
    if show:
        value["showIf"] = dict(setting=show[0], equals=show[1])
    settings.append(value)

def select(key, label, default, choices, description="", show=None):
    setting(key, "select", label, default, description, show,
            options=[dict(value=v, label=l) for v, l in choices])

def slider(key, label, default, lo, hi, step=1, description="", show=None):
    setting(key, "range", label, default, description, show, min=lo, max=hi, step=step)

profiles = [("fluid", "Fluide et expressif"), ("discreet", "Discret · fondu rapide"),
            ("cinema", "Cinéma · ample et progressif"), ("bounce", "Ressort · rebond vivant"),
            ("genie", "Genie · vers le bord de l’écran"), ("wobbly", "Wobbly · déformation expérimentale")]
setting("enabled", "boolean", "Animer les fenêtres", True,
        "Applique les effets aux applications choisies. Désactiver termine les effets et réaffiche les fenêtres.")
select("profile", "Ambiance", "fluid", profiles + [("custom", "Personnalisée")],
       "Les ambiances règlent les quatre mouvements. Choisissez Personnalisée pour régler chaque mouvement et sa durée.")
slider("speed", "Vitesse (%)", 100, 25, 250, 5, "100 % correspond au profil. Une valeur plus élevée accélère les effets.")
slider("intensity", "Amplitude (%)", 100, 0, 200, 5, "Augmente le déplacement, le zoom, le rebond et la déformation sans changer la vitesse.")
section("eventsSection", "Quels moments animer ?")
setting("openEnabled", "boolean", "À l’ouverture", True, "La fenêtre apparaît avec le mouvement choisi. Une apparition initiale peut précéder l’effet.")
setting("closeEnabled", "boolean", "À la fermeture", True, "Anime la dernière capture disponible après la fermeture. Une fenêtre ouverte très brièvement peut être ignorée.")
setting("minimizeEnabled", "boolean", "À la réduction", True, "Anime une capture de la fenêtre vers le bord choisi. L’animation Windows peut aussi se jouer.")
setting("restoreEnabled", "boolean", "À la restauration", True, "Rejoue un mouvement d’apparition quand la fenêtre sort de la barre des tâches.")
section("appsSection", "Applications et fenêtres")
setting("applications", "textarea", "Applications à animer", "notepad.exe",
        "Un nom de programme par ligne, par exemple notepad.exe ou calc.exe. Plusieurs fenêtres de ces programmes sont suivies, y compris si elles s’ouvrent plus tard.", placeholder="notepad.exe\ncalc.exe")
setting("excludedApplications", "textarea", "Applications à laisser tranquilles", "",
        "Un nom de programme par ligne. Cette liste a priorité sur les profils particuliers.")
select("windowTypes", "Types de fenêtres", "normal", [("normal", "Fenêtres principales"), ("dialog", "Boîtes de dialogue"), ("all", "Fenêtres et boîtes de dialogue")],
       "Les fenêtres système et les outils flottants sont ignorés. Le type est déduit de la classe et du propriétaire de la fenêtre.")
setting("previewVisible", "boolean", "Afficher les fenêtres d’essai", False,
        "Ouvre un petit panneau avec Ouvrir, Réduire, Restaurer et Fermer. Les essais utilisent exactement le rendu natif de cet add-on.")
section("motionSection", "Mouvements personnalisés", True)
modes = [("none", "Aucun"), ("fade", "Fondu"), ("slide", "Glissement"), ("zoom", "Zoom"), ("spring", "Ressort"), ("genie", "Genie"), ("wobbly", "Wobbly · expérimental")]
custom = ("profile", "custom")
for name, label, mode, duration in [("open", "Ouverture", "zoom", 280), ("close", "Fermeture", "zoom", 280),
                                    ("minimize", "Réduction", "slide", 360), ("restore", "Restauration", "slide", 360)]:
    select(name + "Mode", label + " · mouvement", mode, modes, show=custom)
    slider(name + "Duration", label + " · durée (ms)", duration, 80, 2000, 10, show=custom)
select("curve", "Progression du mouvement", "out", [("out", "Rapide au départ, douce à l’arrivée"), ("smooth", "Douce au départ et à l’arrivée"), ("inout", "Accélérer puis ralentir"), ("linear", "Vitesse constante")], show=custom)
slider("slideDistance", "Distance du glissement (px)", 54, 0, 400, 2, "Distance à 100 % d’amplitude ; adaptée au DPI de la fenêtre.", custom)
slider("startSize", "Taille de départ du zoom (%)", 86, 10, 100, 1, show=custom)
slider("startOpacity", "Visibilité au début ou à la fin (%)", 8, 0, 100, 1, "0 % disparaît complètement ; 100 % garde l’image opaque.", custom)
slider("bounce", "Force du rebond (%)", 14, 0, 60, 1, show=custom)
slider("damping", "Calmer le rebond", 7, 2, 16, 1, "Une valeur élevée amortit le rebond et la déformation plus vite.", custom)
slider("wobbleAmount", "Souplesse Wobbly (px)", 18, 0, 120, 1, "Déforme une capture pendant la transition ; le déplacement interactif d’une fenêtre n’est pas déformé.", custom)
slider("wobbleFrequency", "Ondulations Wobbly", 3, 1, 8, 1, show=custom)
section("originSection", "Direction et origine")
select("origin", "Origine du mouvement", "bottom", [("bottom", "Bas de l’écran"), ("top", "Haut de l’écran"), ("left", "Bord gauche"), ("right", "Bord droit"), ("center", "Centre de la fenêtre"), ("cursor", "Position du pointeur"), ("taskbar", "Bord de la barre des tâches")],
       "Dirige le glissement et le ressort, et attire Genie vers ce point. Le zoom suit aussi le pointeur ou la barre des tâches. Le bord de la barre est estimé sur l’écran de la fenêtre, sans cibler une icône précise.")
section("lookSection", "Aspect pendant le mouvement", True)
slider("cornerRadius", "Coins arrondis (px)", 12, 0, 48, 1, "Arrondit seulement la capture animée ; l’apparence normale de la fenêtre reste gérée par son application.")
slider("shadowSize", "Ombre douce (px)", 24, 0, 48, 1)
slider("shadowOpacity", "Intensité de l’ombre (%)", 22, 0, 60, 1)
select("borderStyle", "Contour de la capture", "none", [("none", "Sans contour"), ("solid", "Couleur unie"), ("gradient", "Dégradé vertical")])
slider("borderWidth", "Épaisseur du contour (px)", 2, 0, 8, 1)
setting("color1", "color", "Couleur de départ", "#64BFFAFF", alpha=True)
setting("color2", "color", "Couleur d’arrivée", "#DF83FFFF", show=("borderStyle", "gradient"), alpha=True)
section("rulesSection", "Profils particuliers", True)
for i in range(1, 4):
    p = f"rule{i}"
    setting(p + "Enabled", "boolean", f"Activer le profil particulier {i}", False,
            "Le dernier profil activé correspondant à l’application et au type de fenêtre a priorité.")
    show = (p + "Enabled", True)
    setting(p + "Application", "string", f"Application du profil {i}", ["notepad.exe", "calc.exe", "window-animations-preview.exe"][i - 1], "Un nom de programme, par exemple notepad.exe.", show, placeholder="notepad.exe")
    select(p + "WindowType", f"Fenêtres du profil {i}", "all", [("all", "Toutes"), ("normal", "Principales"), ("dialog", "Boîtes de dialogue")], show=show)
    select(p + "Profile", f"Ambiance du profil {i}", "fluid", profiles + [("off", "Aucune animation")], show=show)
    slider(p + "Speed", f"Vitesse du profil {i} (%)", 100, 25, 250, 5, show=show)
section("comfortSection", "Confort et compatibilité")
setting("respectReducedMotion", "boolean", "Respecter les animations réduites de Windows", True,
        "Suspend les effets si Windows désactive les animations. Un changement de ce choix pendant un effet termine immédiatement cet effet.")
setting("skipFullscreen", "boolean", "Ignorer les fenêtres plein écran", True)
setting("hideEntering", "boolean", "Masquer la vraie fenêtre pendant son apparition", True,
        "La capture remplace temporairement la fenêtre lors de l’ouverture et de la restauration. Un processus de surveillance la réaffiche en cas d’arrêt brutal.")
setting("allowTranslucent", "boolean", "Essayer les fenêtres translucides · expérimental", False,
        "La capture de verre, Mica et des applications accélérées peut être vide ou différente de l’original. Un échec de capture laisse la fenêtre visible.")
section("performanceSection", "Fluidité et ressources", True)
slider("frameRate", "Images par seconde visées", 60, 30, 60, 30, "30 réduit le travail de rendu ; 60 privilégie la fluidité. Le débit effectif dépend de la taille de la fenêtre et du processeur.")
slider("meshQuality", "Finesse de Genie et Wobbly", 20, 8, 40, 2, "Une valeur élevée affine les courbes et augmente le coût du rendu.")
slider("maxConcurrent", "Effets simultanés", 3, 1, 4, 1)
slider("maxSnapshots", "Fenêtres gardées en mémoire", 12, 1, 24, 1, "Les captures restent en mémoire, dans une limite totale de 64 Mo. Les fenêtres supplémentaires utilisent Windows.")
slider("cacheInterval", "Actualiser les captures tous les (ms)", 400, 100, 2000, 50, "Les fermetures et réductions utilisent la dernière capture disponible.")
slider("cacheMaxAge", "Âge maximal d’une capture (ms)", 2000, 250, 5000, 50, "Une capture trop ancienne est ignorée pour éviter d’animer un contenu périmé.")
assert len(settings) <= 100
manifest = dict(runtime="native-v1", name="Animations des fenêtres", version="0.1.0", deviceSettingsVersion="1",
                description="Personnalisez les quatre transitions de vos fenêtres avec des ambiances, Genie, des rebonds et des profils par application.",
                settings=settings, native=dict(companion=dict(runtime="process-v2", entries={"windows-x86_64": "native/out/windows-x86_64/companion.exe"})))
(root / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
for name, _ in profiles:
    preset = {s["id"]: s["default"] for s in settings if s["type"] != "section"}
    preset["profile"] = name
    (root / "presets" / f"{name}.json").write_text(json.dumps(preset, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"{len(settings)} definitions, {sum(s['type'] == 'section' for s in settings)} sections, 6 presets")
