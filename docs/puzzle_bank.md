# Format des banques de puzzles

Une banque = un fichier `data/puzzles/<famille>.bin`, produit par
`tools/puzzlegen` (voir `make puzzles`) et embarqué tel quel en ROM par
`tools/bin2c.py` (`build/gen/bank_<famille>.c`). Tout est en petit-boutiste.

| Offset | Taille | Contenu |
|---|---|---|
| 0 | 4 | magic `"DPZ1"` |
| 4 | 1 | famille (`enum PuzzleFamily`, `common/puzzle_common.h`) |
| 5 | 1 | réservé (0) |
| 6 | 2 | `count` : nombre d'enregistrements |
| 8 | 24 | `diff_start[12]` (u16) : index du premier enregistrement de difficulté ≥ d, pour d = 0..11. Les enregistrements sont triés par difficulté croissante, donc `[diff_start[d], diff_start[d+1])` = tous les puzzles de difficulté d |
| 32 | 4 × count | `offset[i]` (u32) : position de l'enregistrement i depuis le début du fichier |
| … | | enregistrements |

Un enregistrement = en-tête commun de 4 octets (`PuzzleHeader`) + charge utile
propre à la famille :

| Octet | Contenu |
|---|---|
| 0 | famille |
| 1 | taille (côté de la grille, ou dimension principale) |
| 2 | difficulté 1..10 (échelle commune à toutes les familles) |
| 3 | flags propres à la famille (DIG : index de la technique la plus dure requise) |

L'ordre à l'intérieur d'un même niveau de difficulté est déterministe (taille,
puis clé canonique), indépendant de la graine : regénérer avec les mêmes
paramètres redonne le même fichier octet pour octet.

## Charges utiles

### DIG (`common/dig.h`) — taille n = 5..8

    region[n*n]   4 bits par case, quartet bas d'abord (id de région 0..n-1)
    solution[n]   4 bits par ligne : colonne de la fouille

Soit (n² + 1)/2 + (n + 1)/2 octets : 15 (5×5) à 36 (8×8).

### VEIN (`common/vein.h`) — n = 6 ou 8

    given[n*n]     2 bits par case (0 libre, 1 clair, 2 sombre), bits bas d'abord
    solution[n*n]  1 bit par case (0 clair, 1 sombre)

Soit n²/4 + n²/8 octets : 14 (6×6), 24 (8×8).

### LEDGER (`common/ledger.h`) — n = 4, 6 ou 8, boîtes 2 × n/2

    given[n*n]     4 bits par case (0 libre, sinon 1..n)
    solution[n*n]  4 bits par case

Soit n² octets : 16, 36, 64.

### TUNNEL (`common/tunnel.h`) — n = 5..7

    cell[n*n]      4 bits par case : 0 libre, 1..k sortie numérotée, 15 roche
    path[n*n-1]    2 bits par pas (0 N, 1 E, 2 S, 3 O) depuis la sortie 1

`flags` de l'en-tête = nombre de sorties k (2..12). Soit (n²+1)/2 + (n²+2)/4
octets : 19 (5×5) à 37 (7×7).

### BLOCK (`common/block.h`) — n = 5..7, longueur variable

    cavity[n*n]    1 bit par case (1 = ouverte), bits bas d'abord
    count          1 octet (3..12 blocs)
    piece[count]   2 octets : forme (5 bits) << 3 | orientation (3 bits), ancre de la
                   solution (case en haut à gauche de la boîte englobante orientée)

`flags` = nombre de blocs. Formes : catalogue des 19 polyominos libres de 3 à 5
cases (`common/block.c`), orientations distinctes énumérées dans l'ordre des
8 transformations. Le lecteur ROM calcule la longueur à partir de `count`.

### HEART (`common/heart.h`) — n = 10, 12, 15

    picture[n*n]   1 bit par case (1 = minerai), bits bas d'abord
    given[n*n]     1 bit par case (1 = révélée au départ)

`flags` = nombre de cases révélées. Longueur : 26 (10×10), 36 (12×12),
58 (15×15). Difficulté fixée par la taille (4, 7, 10) pour que la fenêtre
[d−1, d+1] du constructeur de run ne mélange jamais deux tailles. Les images
viennent de `assets/heart/*.png` via `tools/heart_pictures.py` ;
`--count 0` laisse le générateur garder chaque dessin une fois.

### NUGGET

Pas de banque : la salle bonus est tirée à l'exécution de la graine de run
XOR la profondeur (`source/fam_nugget.c`).

## Génération

`make puzzles` appelle `puzzlegen` avec une graine fixée par famille dans le
`Makefile`. Chaque candidat passe trois filtres : solution unique (compteur
plafonné à 2), résolution complète par le solveur de déduction pour DIG, VEIN
et LEDGER (jamais d'essai-erreur ; TUNNEL et BLOCK mesurent l'effort de leur
recherche élaguée, voir `docs/design.md`) et clé canonique inédite (forme
minimale parmi les symétries admissibles du carré, étiquettes renumérotées par
ordre d'apparition, plus les symétries propres à la famille). L'option
`--per-diff K` plafonne chaque niveau de difficulté pour aplatir la
répartition ; `--dump` imprime chaque puzzle accepté en texte. Les comptes
demandés (800 fouilles, 600 filons, 500 carnets, 500 galeries, 500 cavités)
sont dimensionnés pour que **chaque niveau 1–10 soit plein** (80, 60 ou 50
puzzles) : les paliers profonds d'une descente de 60 piochent à 9–10 sans
retomber sur les mêmes grilles. `make puzzles` écrit ensuite le décompte par
taille et difficulté dans [`docs/banks.md`](banks.md) (`tools/bank_report.py`).
