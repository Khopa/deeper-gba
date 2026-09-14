# Deeper — notes de conception

État des lieux de ce qui est construit, des décisions prises et de ce qui
reste ouvert. Le prompt d'origine (sections 1 à 12) sert de référence ; les
numéros ci-dessous y renvoient.

## Architecture (§10)

Trois compilations du même code de règles :

| Cible | Sources | Rôle |
|---|---|---|
| ROM (`make`) | `source/` + `common/` + banques + assets générés | le jeu |
| `puzzlegen` (`make puzzlegen`) | `tools/puzzlegen/` + `common/` | génération, unicité, difficulté, dédoublonnage |
| tests (`make test`) | `tests/unit/` + `common/` + modules moteur sans matériel | vérification |

`common/` ne touche ni au matériel ni aux entrées-sorties : chaque famille y
définit ses règles (conflits, victoire), son format compact, son solveur de
déduction (ou sa recherche élaguée) et sa mesure de difficulté. Les adaptateurs
`source/fam_*.c` traduisent ces règles en actions curseur/A/B/R pour l'écran
de salle (`source/room.c`) via l'interface `include/puzzle.h`, sans dessiner
eux-mêmes : ils décrivent chaque case (`CellView` : jeu de tuiles, bords épais,
palette, marque) et la salle les dessine. C'est ce qui permet de tester les
adaptateurs sur PC.

Mémoire : ROM d'environ 110 Ko (dont 60 Ko de banques), état de run ~650 octets,
sauvegarde SRAM < 2 Ko. Aucune allocation dynamique côté GBA.

## Familles (§3)

| Famille | Nom joueur | Grille | Solveur | Difficulté |
|---|---|---|---|---|
| DIG | Fouilles | 5–8 | déduction : single, confinement, paires, essai à un coup | technique la plus dure + longueur des chaînes |
| VEIN | Filons | 6, 8 | déduction : voisins forcés, comptage, essai local | part de cases vides, comptages, essais |
| LEDGER | Carnet | 4, 6, 8 (boîtes 2 × n/2) | déduction : singles cachés, singles nus, candidats verrouillés | part de cases vides, tenue de candidats, taille |
| TUNNEL | Galerie | 5–7 | recherche élaguée (connexité, impasses) | effort de recherche + densité des sorties |
| BLOCK | Cavité | 5–7 | couverture exacte avec brisure de symétrie | effort de recherche + nombre de blocs |

Cavité : le bloc courant est prévisualisé en fantôme sous le curseur (doré
s'il rentre, rouge sinon), R le tourne, B passe au suivant, A le pose (coin
haut-gauche de sa boîte englobante sur le curseur) ou reprend un bloc posé.
| NUGGET | Pépites | 6 | aucun (salle bonus, tirée de la graine de run) | — |
| HEART | Le Cœur | 10, 12, 15 (noyau seulement) | résolution ligne par ligne (énumération des placements) | fixée par la taille : 4, 7, 10 |

Le Cœur (`common/heart.c`, `source/fam_heart.c`) est la salle finale et
n'apparaît nulle part ailleurs : les nombres autour de la grille donnent,
dans l'ordre, les suites de minerai de chaque ligne et colonne ; A marque le
minerai, B note la roche, une ligne satisfaite passe en gris. Les images sont
des dessins (`assets/heart/*.png`, 1 bit) et non des grilles aléatoires : le
générateur garde celles dont les indices tiennent à l'écran (7 caractères par
ligne, 4 ou 5 rangées au-dessus des colonnes) et, quand un dessin ne se
résout pas ligne à ligne, pré-révèle les quelques cases qui y suffisent (choix
glouton : la case qui en fixe le plus) — c'est ce qui garantit l'unicité. La
taille suit la longueur de la descente (15 → 10 × 10, 30 → 12 × 12,
60 → 15 × 15), le budget de temps est triplé, un minerai posé sur de la roche
est un conflit à retardement comme ailleurs, l'indice révèle jusqu'à cinq
cases de la ligne la plus proche d'être finie. Une fois complète, l'image
s'allume sur fond sombre et pulse pendant la célébration. Cases de 8 px
(`cells_small.png`, curseur `cursor_small.png`), disposition dédiée dans
`room.c` (indices à gauche et au-dessus, panneau compact, en-tête sur une
ligne, lignes de guidage tous les cinq cases sur le canevas passé au-dessus
des cases).

Toutes les banques garantissent l'unicité de la solution (compteur plafonné
à 2). Pour DIG, VEIN et LEDGER, le générateur exige en plus que le solveur de
déduction termine sans essai-erreur : la difficulté découle des techniques
réellement nécessaires. Pour TUNNEL et BLOCK, où le raisonnement humain est de
la planification plutôt que de l'élimination nommée, la difficulté est un
proxy : le nombre de nœuds visités par la recherche élaguée, calibré sur les
distributions observées (voir les seuils dans `common/tunnel.c` et
`common/block.c`). C'est le point le plus à affiner par le playtest.

Dédoublonnage : forme canonique = minimum sur les symétries du carré
admissibles (les 8 pour les grilles carrées, 4 pour les boîtes 2 × 3 du
carnet), régions/symboles renumérotés par ordre d'apparition, plus les
symétries propres à la famille (échange des deux minerais, inversion du sens
de la galerie, multi-ensemble de formes des blocs). Clé FNV-64 dans un
ensemble de hachage.

Répartition actuelle (`make puzzles`, graines dans le `Makefile`) :

    dig     480  difficultés 1-10, tailles 5:311 6:120 7:38 8:11
    vein    320  1-10 (peu de 8), 6:149 8:171
    ledger  320  1-10 (surtout 3-7), 4:115 6:85 8:120
    tunnel  320  1-10, 5:139 6:146 7:35
    block   320  1-10, 5:219 6:88 7:13

## Run et carte (§4, §5)

`source/run.c` : 15, 30 ou 60 paliers (`run_length`) × 3 emplacements ; les
tableaux sont dimensionnés pour 60. Une descente plus longue se débloque en
atteignant le noyau de la précédente (`Profile.lengths_unlocked`, écran de
longueur après « Nouvelle descente »). Trois chemins monotones ordonnés (pas
de croisement) tracent le graphe ; leur union donne les nœuds. Le palier 0
(entrée) est toujours une salle de fouilles ; le dernier palier est le noyau
(Le Cœur, difficulté 4/7/10 selon la longueur, récompense triple ; fouilles à
difficulté 10 si la banque du Cœur manque). Au tiers et aux deux tiers de la
descente, **tout le palier est un campement** (+1 vie, comptoir) : quel que
soit le chemin, chaque run a ses deux haltes. Échouer au noyau coûte une vie
comme ailleurs puis le Cœur est proposé de nouveau (une autre image de la
même taille quand la banque en a) tant qu'il reste des vies ; la défaite
n'arrive qu'à zéro vie. Types de nœuds : normal, risqué (difficulté +2, stabilité 2,
minerai double, jamais avant le palier 4), indice (+1 jeton), vie (+1),
pépites (12 % des nœuds normaux à partir du palier 2).

Courbe : `run_base_difficulty(l, L) = 1 + 8·l/(L−1) + {0,+1,0,−1}[l mod 4]`,
plafonnée à 2 sur les trois premiers paliers quelle que soit la longueur L. Chaque nœud tire un puzzle de
sa famille dans la fenêtre [d−1, d+1], élargie progressivement si la banque
n'a rien à ce niveau, en évitant les 64 derniers puzzles joués (anneau dans le
profil). La famille d'un nœud évite celles du palier précédent quand c'est
possible ; les fouilles sont deux fois plus probables (mécanique signature).

Écran de carte : fenêtre de cinq paliers, nœud courant en haut, lignes tracées
sur un calque pixel (BG2 utilisé comme canevas 4 bpp plein écran). La ligne du
bas prévisualise le nœud surligné : famille, difficulté en étoiles (d/2),
minerai, bonus, étiquette RISQUE. Animation de marche du nain vers le nœud
choisi.

## Ressources, pouvoirs, sauvegarde (§6, §7)

- Vies : **une seule, maximum une**, au départ ; le comptoir vend le maximum
  (paillasse, jusqu'à 5) puis les vies de départ (gourde, jusqu'à 3, jamais
  au-dessus du maximum). Campements et salles « vie » soignent jusqu'au
  maximum de la run, pas au-delà. Indices (3, max 9), minerai (récompense =
  10 + 5·d, moins 5 par indice et 2 par erreur, plancher au quart, **plus un
  bonus de vitesse** = récompense × temps restant / budget). Le budget d'une
  salle est `run_time_budget(d) = (45 + 15·d) / 2` secondes (30 s à 97 s),
  que la lanterne allonge de +50 / +100 / +150 % selon son niveau ; une barre
  de 128 px sous la grille (calque BG2) se vide en temps réel, passe au rouge
  dans le dernier dixième, et l'écran flashe alors toutes les deux tiers de
  seconde jusqu'à ce que le budget soit épuisé (le bonus, lui, n'est pas une
  condition de réussite). Le temps de la salle est
  sauvegardé avec elle (`RoomSave.elapsed`, sauvegarde périodique toutes les
  10 s) ; le temps total de la descente (`RunState.frames`, carte comprise)
  est affiché à la fin et conservé dans le profil : dernier temps et record
  par longueur (victoires seulement).
- Stabilité par salle : 4 (normale), 2 (risquée), 3 (indice/vie/noyau),
  6 tentatives pour les pépites. Un conflit visible (creusement en conflit,
  triple de minerai, doublon) ne coûte rien s'il est corrigé dans les deux
  secondes : la case clignote à partir d'une seconde, puis explose et
  consomme un point de stabilité, une seule fois tant qu'elle reste fausse
  (`room.c`, `MISTAKE_DELAY`). Les erreurs ponctuelles (case interdite à côté
  de la tête de galerie, roche nue en prospection) coûtent immédiatement.
  À zéro : éboulement, −1 vie ; les salles bonus se terminent simplement et
  paient ce qui a été ramassé.
- SELECT : abandonner la salle contre une vie.
- Pouvoirs permanents : Lampe (+1 indice par descente, profondeur 10 atteinte),
  Endurance (+1 vie, profondeur 20), Seconde chance (premier éboulement
  gratuit, noyau atteint). Ce sont des aides, jamais des résolveurs.
- **Boutique** (`source/shop.c` logique, `source/shopscreen.c` écran) : le
  marchand Barnabé (sprite 32 × 32 affiché doublé par matrice affine, derrière
  un comptoir dessiné sur le canevas) tient deux étals. Au campement (après le
  +1 vie), payés en minerai de run et à prix fixe : indice (30), vie (60),
  étai (25, +2 de stabilité à la prochaine salle, jusqu'à 3 en stock). Au menu
  titre, payés avec `ore_bank` (le minerai remonté, cumulé), prix croissant
  par niveau : sacoche (200/400, +1 indice de départ par niveau), paillasse
  (200/400/600/800, +1 vie maximum par niveau, jusqu'à 5), gourde (300/600,
  +1 vie de départ par niveau, jusqu'à 3, verrouillée tant que le maximum ne
  suit pas), lanterne (300/600/900, temps +50/+100/+150 %), casque doré et
  barbe rousse (150 chacun, recolorent le nain du joueur). Tout est dans le
  profil (`upgrade[]`, `cosmetics`), les étais dans la run (`props`).
- SRAM : bloc profil à 0x000, bloc run à 0x400, chacun {magic, version,
  longueur, FNV-32} + charge. Le bloc run est réécrit à chaque issue de salle
  et à chaque modification de plateau (sauvegarde de salle : ≤ 96 octets par
  famille + stabilité, indices, compteurs, curseur).

## Entrée, sortie et bilan d'une salle

Une salle s'ouvre par une animation : les cases apparaissent diagonale par
diagonale (`ST_INTRO`, deux images par diagonale), le curseur ensuite ; les
touches et le chrono attendent. Résolue, la grille est balayée en doré
diagonale par diagonale puis s'efface de la même façon (le Cœur, lui, pulse
son image), et un **bilan** modal s'affiche : gemmes (trois : sans erreur ni
indice et plus de la moitié du budget restant ; deux : sans erreur ; une
sinon), barre du temps restant, erreurs et indices, récolte de base, bonus
de vitesse, pénalités, puis le minerai gagné **compté en montant** (A saute
le compte, A ensuite continue). Le chrono de run ne tourne ni pendant
l'entrée ni pendant le bilan.

## Rendu (§8)

Mode 0. BG0 = texte + marques (fonte 8 × 8, marques 16 × 16), BG1 = cases
16 × 16 (32 métatuiles : 16 variantes de bords épais + 16 galeries creusées
selon les côtés reliés), BG2 = canevas pixel (carte, barre de temps,
comptoir), BG3 = fond de biome (six variantes 64 × 64 tirées au sort dans la
planche du biome et pavées au hasard, `assets/back_*.png`, dérive lente sur la
carte, assombri par le matériel), sprites = curseur, nain (16 × 16, 2 images par
animation) et marchand (32 × 32 doublé). Les couleurs sont des banques de
palette : 8 banques de roche pour les régions/pièces, une banque « conflit »,
une « tête/pépite », banques de texte (blanc, gris, doré, rouge ; les lignes du
canevas empruntent la banque rouge, le fond garde ses 15 couleurs). Les cartes
d'écran occupent la fin du bloc de caractères 1, le bloc 3 étant entier pour
le débord du canevas et les variantes de fond. Six biomes
selon la profondeur (terre, roche, glace, lave, cristal, noyau) teintent le
fond et l'accent et désignent une piste musicale.

Animations en place : nain (repos/creusement), curseur pulsant, marche sur la
carte, célébration de fin de salle, éboulement, écrans de fin de run. Pistes
d'amélioration : étincelles sur la case résolue, transition de biome, sprite
de nain plus grand et animé selon la famille.

## Son (§9)

`source/sound.c` : dix effets PSG (déplacement, action, note, erreur, indice,
salle dégagée, éboulement, collecte, pas, pouvoir) sur un petit séquenceur
par image. `source/music.c` : pistes PCM 8 bits 10,5 kHz lues en ROM par DMA1
vers DirectSound A (`docs/assets.md`), bouclées au comptage d'images ;
redemander la piste en cours (biomes voisins) ne la redémarre pas. Pistes
provisoires converties de *Dwarves Manager* ; l'option son coupe aussi la
musique.

## Tests (§11)

`tests/README.md`. 38 000 vérifications sur PC en moins d'une seconde
(règles, emballage, solveurs, générateurs, banques engagées dans le dépôt,
carte de run, sauvegarde, adaptateurs, sons), sept scénarios mGBA couvrant le
démarrage, une salle résolue depuis la banque, la carte, l'abandon, la reprise
exacte après reset, le délai avant pénalité et l'éboulement, la défaite, un
soak aléatoire et une descente complète sur graine fixe (toutes les familles
résolues depuis les banques ROM, noyau atteint sans perdre de vie), dont
`docs/fullrun.gif` est l'enregistrement (`make fullrun`).

## Ouvert

- Calibrage de la difficulté par playtest, en particulier TUNNEL et BLOCK.
- Boutique et cosmétiques financés par `ore_bank`, reliques (§7).
- Assets finaux et musique (§8, §9) ; le nain de *Dwarves Manager* comme
  référence (`E:\Work\dw2\android\assets\data\gfx\hd\sheets\characters.png`).
- Indices à plusieurs forces (aujourd'hui : un pas de déduction par jeton ;
  le pointage d'une erreur est gratuit).
- Écran de langue à chaque démarrage (FR/EN) ; la langue et le son se changent
  aussi dans Options, mais l'écran de démarrage n'est pas encore désactivable.
- Une piste par biome (aujourd'hui deux pistes se partagent les six biomes).
