# Pipeline d'assets : du prototype au final

Le code ne connaît que des noms de fichiers et des dispositions de tuiles.
Remplacer un PNG de `assets/` par sa version définitive, avec le même nom, la
même taille et les mêmes index de palette, suffit : `make` le reconvertit.

## Chaîne

    assets/<nom>.png (+ assets/<nom>.opts)  --png2gba.py-->  build/gen/gfx_<nom>.c/.h
    data/puzzles/<famille>.bin             --bin2c.py---->  build/gen/bank_<famille>.c/.h

`tools/make_assets.py` redessine les PNG provisoires depuis de l'ASCII art
(`make assets`) ; il n'est plus nécessaire une fois les assets finaux en place.
`png2gba.py` accepte une image indexée (mode « P », index < 16) ou une image
couleur (≤ 16 couleurs, le magenta #FF00FF ou la première couleur devient
l'index 0 transparent). `--meta 2 2` (dans le `.opts`) émet les tuiles par
métatuile 16 × 16.

## Fichiers et conventions d'index

| Fichier | Disposition | Index de palette attendus |
|---|---|---|
| `font.png` | bande de glyphes 8 × 8, ordre `FONT_CHARS` (`tools/make_assets.py` et `source/render.c`) | 1 = encre. La couleur vient de la banque de texte |
| `cells.png` | 32 métatuiles 16 × 16 : 0–15 roche (bits de bords épais 1 N, 2 E, 4 S, 8 O), 16–31 galerie creusée (bits des côtés reliés) | 1 remplissage, 2 biseau clair, 3 biseau sombre, 4 bord/encre. Chaque région de roche reçoit sa banque (`region_palette()`) |
| `marks.png` | métatuiles 16 × 16 : vide, croix, fouille, alerte, gemme, minerai clair, minerai sombre, puis nombres 1–12 | 1 encre sombre, 2 encre claire, 3 alerte, 4 or |
| `nodes.png` | icônes de la carte : fouille, filon, bloc, galerie, carnet, pépite, campement, noyau, indice, vie, risque | palette libre (≤ 15 couleurs) ; les états (atteignable, passé, lointain) sont des variantes en gris calculées |
| `cursor.png` | sprite 16 × 16, 2 images | 1 = blanc |
| `dwarf.png` | sprite 16 × 16, 4 images : repos A/B, creusement A/B | palette libre ; copiée dans la banque OBJ 1. Les cosmétiques de la boutique remplacent l'index 5 (casque) et 3 (barbe) à l'exécution |
| `merchant.png` | sprite 32 × 32, 2 images (repos, hochement), affiché doublé (64 × 64) | palette libre ; banque OBJ 3 |
| `back_<biome>.png` (earth, rock, ice, lava, crystal, core) | bloc 32 × 32 répété sur tout l'écran (BG3), `--meta 4 4` | **indices 4 à 15 seulement** (1–3 servent aux lignes du canevas dans la même banque) ; opaque partout ; rester sombre pour la lisibilité du texte |

Contraintes : tout en 4 bpp ; les tuiles des cases doivent rester lisibles
sous n'importe laquelle des huit teintes de roche (`region_fills` dans
`render.c`) puisque la couleur est appliquée à l'exécution ; les marques se
superposent aux cases (index 0 transparent).

## Références *Dwarves Manager*

Sprites d'origine : `E:\Work\dw2\android\assets\data\gfx\hd\sheets\characters.png`
(nains carrés à grande barbe, casques et chevelures), palette
`E:\Work\DwarvesManagerII_GFX\gfx\UI_Area\DwarvesPalette.png`, tuiles de sol
`.../pixelArt/tileset.png`. Le nain provisoire reprend la silhouette (tête
carrée, barbe large, casque rond) en 16 × 16.

## Son

Effets : `source/sound.c` (tables de pas PSG, faciles à retoucher). Musique :
`music_play(MusicId)` est appelé par chaque écran ; brancher un lecteur
derrière cette fonction (et `sound_update()` par image) est la seule
modification attendue. Les pistes prévues : carte, un thème par biome (terre,
roche, glace, lave, cristal, noyau), victoire, défaite.
