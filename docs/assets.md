# Pipeline d'assets : du prototype au final

Le code ne connaît que des noms de fichiers et des dispositions de tuiles.
Remplacer un PNG de `assets/` par sa version définitive, avec le même nom, la
même taille et les mêmes index de palette, suffit : `make` le reconvertit.

## Chaîne

    assets/<nom>.png (+ assets/<nom>.opts)  --png2gba.py-->  build/gen/gfx_<nom>.c/.h
    data/puzzles/<famille>.bin             --bin2c.py---->  build/gen/bank_<famille>.c/.h

`tools/make_assets.py` redessine les PNG provisoires depuis de l'ASCII art,
puis `tools/import_concept.py assets/concept_sheet.png` découpe la planche de
concept-art (une seule grande image de maquette) et écrase les PNG qu'elle
couvre : écran titre, icônes du menu, touches GBA, marchand, nain, icônes de
carte, marques fouille/gemme/minerai (aperçu dans `build/concept_preview.png`) ;
enfin `tools/import_tiles.py` convertit les **planches de tuiles de biome**
(`assets/high-res/tiles/<planche>.png` : une grille 4 × 4 de tuiles carrées
séparées par des gouttières noires de 1 px, soit 131 × 131 pour du 32 px,
comme Pixel Lab les exporte — ou une simple bande) en `back_<biome>.png`. La
table `SHEETS` en tête du script dit quelle planche sert à quel biome
(ground, stone, ice, lava, crystal, core). `make assets` enchaîne tout.
Chaque découpe est une boîte `(x0, y0, x1, y1)` en tête du script : déplacer
un élément sur la planche = corriger la boîte. Le fond uni autour d'un sprite
est détouré par remplissage depuis le bord, chaque sprite est réduit à la
taille de son emplacement puis quantifié dans le budget de couleurs de sa
banque. Les PNG finaux dessinés à la main remplacent simplement ceux-là
(mêmes noms, mêmes tailles, mêmes index).

`png2gba.py` accepte une image indexée (mode « P », index < 16) ou une image
couleur (≤ 16 couleurs, le magenta #FF00FF ou la première couleur devient
l'index 0 transparent). `--meta 2 2` (dans le `.opts`) émet les tuiles par
métatuile 16 × 16 ; `--bitmap` émet une image 8 bpp plein écran (mode 4) avec
sa palette de 256 couleurs, pour l'écran titre.

## Fichiers et conventions d'index

| Fichier | Disposition | Index de palette attendus |
|---|---|---|
| `font.png` | bande de glyphes 8 × 8, ordre `FONT_CHARS` (`tools/make_assets.py` et `source/render.c`) | 1 = encre. La couleur vient de la banque de texte |
| `cells.png` | 32 métatuiles 16 × 16 : 0–15 roche (bits de bords épais 1 N, 2 E, 4 S, 8 O), 16–31 galerie creusée (bits des côtés reliés) | 1 remplissage, 2 biseau clair, 3 biseau sombre, 4 bord/encre. Chaque région de roche reçoit sa banque (`region_palette()`) |
| `marks.png` | métatuiles 16 × 16 : vide, croix, fouille, alerte, gemme, minerai clair, minerai sombre, fantômes, éclats, puis nombres 1–12 | palette libre (≤ 15 couleurs + transparent), prise telle quelle par le moteur. **Dessiner la bande entière dans `assets/high-res/marks.png`** (magenta ou alpha = transparent, autant de couleurs qu'on veut : l'import la ramène à 15) ; fouille, gemme et les deux minerais sont ensuite pris dans la planche Pixel Lab (`import_icons.py`) ; sinon des marques isolées dans `assets/high-res/marks/<nom>.png` remplacent celles de la planche de concept |
| `nodes.png` | 23 icônes 16 × 16 : les 11 de la carte (fouille, filon, bloc, galerie, carnet, pépite, campement, noyau, indice, vie, risque), les 9 marchandises du comptoir (ordre de `enum ShopItem`), puis caisses, combat, paroi ; choisies dans la planche Pixel Lab `assets/pixelab.png` par la table de `tools/import_icons.py` | palette libre (≤ 15 couleurs) ; les états de la carte (atteignable, passé, lointain) sont des variantes en gris calculées ; les 11 premières vont aussi en mémoire sprite (barre du haut des salles) |
| `cursor.png` | sprite 16 × 16, 2 images | 1 = blanc |
| `dwarf.png` | sprite 16 × 16, 4 images : repos A/B, creusement A/B | palette libre ; copiée dans la banque OBJ 1. Les cosmétiques de la boutique remplacent l'index 5 (casque) et 3 (barbe) à l'exécution |
| `merchant.png` | sprite 32 × 32, 2 images (repos, hochement), affiché doublé (64 × 64) | palette libre ; banque OBJ 3 |
| `back_<biome>.png` (earth, rock, ice, lava, crystal, core) | bande de 1 à 16 variantes d'un bloc 64 × 64 (BG3), `--meta 8 8` ; à chaque écran le moteur charge six variantes tirées au sort et en pave l'écran au hasard (blocs 64 × 64) | indices 1 à 15 (15 couleurs partagées par toute la bande, choisies « la plus éloignée d'abord » pour garder les teintes rares) ; opaque partout. Le matériel assombrit BG3 (`BACKDROP_FADE` dans `render.c`), inutile de foncer l'image |
| `cells_small.png` | 6 tuiles 8 × 8 pour Le Cœur : roche inconnue, minerai, note de roche (croix), aplat index 1, minerai allumé, aplat index 2 | mêmes index que `cells.png` ; banques : roche sombre (région 0), note grise (région 6), minerai (surbrillance), aplat sombre (banque grise) |
| `cursor_small.png` | 2 images 8 × 8 (pulsation) puis 2 éclats de roche (les particules de la paroi) | 1 = blanc, 2 = brun, 3 = gris (banque OBJ 0) |
| `assets/heart/<nom>.png` | **les images du Cœur** : carrées, 10, 12 ou 15 px, n'importe quel mode ; pixel opaque et sombre = minerai | pas de palette : 1 bit. Une image dont une ligne a trop de suites (plus de 7 caractères d'indices, plus de 4–5 rangées) est ignorée avec un avertissement à `make puzzles` |
| `foe_<goblin,orc,troll,demon>.png` | 9 images 64 × 64 (boucle d'attente), issues de `assets/high-res/foes/<nom>-idle.png` (grille 3 × 3 de 128 px) par `tools/import_foes.py` | palette libre (15 couleurs par monstre) ; banque OBJ 3, chargé dans la zone du marchand au début du combat |
| `title.png` | 240 × 160, image de l'écran titre (mode 4), `--bitmap` ; le texte « appuyez sur START » est un sprite clignotant, pas incrusté | ≤ 255 couleurs, index 0 inutilisé |

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

| `logo.png` | 128 × 32, `--meta 8 4` (deux sprites 64 × 32) | 1 ombre, 2 corps, 3 rehaut ; banque OBJ 5 |
| `menu_icons.png` | 5 icônes 32 × 32 : continuer, nouvelle descente, comptoir, carnet, options | palette libre ; banque OBJ 4 (allumée) et 6 (version grisée calculée) |
| `buttons.png` | 11 icônes 16 × 16 : A, B, L, R, START, SELECT, croix, puis les flèches haut/bas/gauche/droite (dessinées ; les combats les affichent) | **indices 2 à 15** (l'index 1 est l'encre du texte blanc, même banque) |

## Son

Effets : `source/sound.c` (tables de pas PSG, faciles à retoucher).

Musique : **déposer un WAV dans `assets/music/`** (PCM 8/16/24/32 bits,
mono ou stéréo, n'importe quelle fréquence) ; `tools/wav2gba.py` le
mixe en mono, le rééchantillonne à 10 512 Hz et le convertit en 8 bits
signés → `build/gen/mus_<nom>.c`. Pistes (`source/music.c`, `MusicId`) :
`title` (menu, marchand, campements), `descent` (la carte), `mine`,
`puzzle` et `puzzle2` (les salles en tirent un au hasard, jamais le même
que la salle précédente), `core` (le Cœur), `combat` (rencontres),
`jingle_victory`, `jingle_defeat`. Un fichier dont le nom commence par
`jingle` se joue une fois, les autres bouclent. Lecture : DirectSound A
alimenté par DMA1 directement depuis la ROM, cadencé par le timer 0
(diviseur 1596). Coût ROM : 10,5 Ko par seconde ; les sept thèmes (~17 min)
pèsent 11 Mo. Les WAV du dépôt sont déjà au format ROM (8 bits mono
10 512 Hz, `wav2gba.py --wav`), 1,7 Mo chacun au lieu de 30 : garder les
originaux 48 kHz hors dépôt, la conversion donne les mêmes octets.

