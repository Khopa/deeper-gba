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

## Génération

`make puzzles` appelle `puzzlegen` avec une graine fixée par famille dans le
`Makefile`. Chaque candidat passe trois filtres : solution unique (compteur
par retour arrière), résolution complète par le solveur de déduction (jamais
d'essai-erreur) et clé canonique inédite (forme minimale parmi les 8
symétries du carré, régions renumérotées par ordre d'apparition). L'option
`--per-diff K` plafonne chaque niveau de difficulté pour aplatir la
répartition.
