# Prompt Pixel Lab — les marques (`assets/marks.png`)

Les marques sont les symboles posés sur les cases des salles : 16 × 16 px,
fond transparent, superposées aux cases de roche (beiges/brunes) ou aux
galeries creusées. Pixel Lab travaille une image à la fois : un **préfixe de
style** commun, puis une ligne par marque. Générer en 16 × 16 directement
(ou 32 × 32 puis réduire ×2 sans lissage) ; enregistrer chaque résultat sous
`assets/high-res/marks/<nom>.png` et lancer `make assets` — l'import remplace
la marque de ce nom (fond transparent détouré, quantifié avec les autres
marques importées sur 11 couleurs partagées).

## Préfixe de style (à coller devant chaque prompt)

```
16x16 pixel art game icon, GBA style, chunky readable shapes, 1px dark brown
outline (#2a1a10), warm earthy palette matching a dwarf mining game (browns,
ochre, gold, a little teal and red), strong contrast, no anti-aliasing, no
gradients, flat colours, at most 8 colours, fully transparent background,
centred, fills about 12x12 of the 16x16 canvas, no text, no border, no
drop shadow.
```

## Les marques

| Fichier | Prompt (après le préfixe) | Rôle dans le jeu |
|---|---|---|
| `cross.png` | `a small wooden X made of two crossed dark planks, marking "not here"` | note « pas ici » (B) dans les fouilles |
| `dig.png` | `a steel pickaxe head striking, small sparks, seen from the front` | une fouille posée (A) |
| `alert.png` | `a bold red exclamation mark on a small round warning sign` | conflit visible |
| `gem.png` | `a faceted teal gemstone, bright highlight top-left` | pépite / gemme |
| `ore_light.png` | `a small pile of three gold ore nuggets, light and shiny` | minerai clair (filons) |
| `ore_dark.png` | `a small pile of three dark iron ore lumps, dull grey with a blue tint` | minerai sombre (filons) |
| `ghost_ok.png` | `a dotted square outline only, thin green dots, hollow centre` | fantôme du bloc qui rentre |
| `ghost_bad.png` | `a dotted square outline only, thin red dots, hollow centre` | fantôme du bloc qui ne rentre pas |
| `burst1.png` | `a small explosion, frame 1 of 3: tight bright yellow flash with a few rock chips` | éboulement, image 1 |
| `burst2.png` | `a small explosion, frame 2 of 3: orange burst spreading outward, rock chips flying` | image 2 |
| `burst3.png` | `a small explosion, frame 3 of 3: fading grey smoke puffs and scattered rock dust` | image 3 |

Les nombres 1 à 12 (carnet de prospection) restent dessinés par
`make_assets.py` : ce sont des glyphes, pas des icônes.

## Contraintes à vérifier au retour

- Exactement 16 × 16, fond transparent (alpha), rien qui touche le bord
  (une marge de 1–2 px : les marques se superposent à des cases de 16 px).
- Lisible sur une case beige **et** sur une galerie sombre : c'est le rôle
  du contour brun foncé.
- Les trois `burst` forment une suite (même centre, tailles croissantes).
- Les deux `ghost` doivent être creux (le contenu de la case reste visible).
- Palette : l'import ramène l'ensemble des marques importées à 11 couleurs ;
  rester dans une famille de teintes cohérente évite les surprises.

Aperçu après import : `build/concept_preview.png` (ligne « mark … »).
