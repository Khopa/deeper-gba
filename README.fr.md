# Deeper

Un puzzle roguelike pour Game Boy Advance, spin-off de *Dwarves Manager*
(Android, 2013). Des nains creusent le sous-sol descente après descente : une
trentaine de salles de puzzles logiques sur une carte à embranchements, de la
surface au noyau terrestre.

*English version: [README.md](README.md).*

![Une descente complète, graine 20260913, jouée par le banc de test](docs/fullrun.gif)

*Une descente entière de 15 paliers, de la surface au noyau, enregistrée depuis mGBA (`make fullrun`).*

## Ce que contient la ROM

- **Six types de salles**, une seule identité visuelle : fouilles (une par
  ligne, colonne et roche, jamais côte à côte), filons de minerai à équilibrer,
  carnets de prospection (chaque symbole une fois par ligne, colonne et zone),
  galeries continues passant par chaque case avec des sorties numérotées,
  cavités à remplir de blocs de pierre, et salles bonus de pépites.
- **Une descente à embranchements** de 15, 30 ou 60 paliers (chaque longueur
  se débloque en atteignant le noyau de la précédente) : à chaque nœud la carte
  annonce les salles suivantes avec leur type, une jauge de difficulté et leur
  récompense (minerai, indice, vie, salle à risque payant double, campement).
  La difficulté monte en dents de scie et démarre toujours en douceur.
- **Contre la montre** : une barre sous chaque puzzle se vide au fil du budget
  de temps de la salle ; plus on la dégage vite, plus le bonus de minerai est
  grand (jusqu'à doubler la récompense). Le temps total de chaque descente est
  conservé, avec un record par longueur dans le carnet.
- **Ressources de run** : vies, indices, minerai. Un conflit laissé en place
  deux secondes entame la stabilité de la salle (avec une petite explosion) ;
  corrigé à temps, il ne coûte rien. À zéro, la salle s'éboule et coûte une
  vie. Un indice applique une vraie étape de déduction, jamais la solution.
- **Le comptoir de Barnabé** : un nain marchand tient chaque campement (un
  indice, une vie ou un étai pour la salle suivante, payés avec le minerai de
  la descente) et son comptoir au menu titre, où le minerai remonté achète de
  l'équipement permanent (sacoche, gourde, lanterne) et des cosmétiques pour
  votre nain.
- **Progression permanente**, gagnée uniquement en jouant : statistiques,
  carnet du mineur et trois pouvoirs débloqués par paliers (un indice de plus
  par descente, une vie de plus, un premier éboulement gratuit).
- **Six biomes** le long de la descente, chacun avec son propre fond derrière
  les salles et la carte.
- **Sauvegarde à pile** : la descente en cours (jusqu'au plateau, au curseur et
  aux compteurs de la salle en cours) et le profil permanent sont deux blocs
  SRAM indépendants avec somme de contrôle. *Continuer* rouvre la salle exacte.
- **Banques de puzzles** générées sur PC : chaque puzzle a une solution unique,
  une difficulté mesurée de 1 à 10 et aucun quasi-doublon aux symétries du
  carré près. Environ 1760 puzzles dans cinq familles, 60 Ko de ROM.

Graphismes provisoires, effets PSG et musiques converties de *Dwarves Manager*
pour l'instant ; le pipeline est prévu pour que les assets finaux (PNG, WAV)
les remplacent sans toucher au code ([docs/assets.md](docs/assets.md)).

## Compilation

C pur sur devkitARM + libtonc, sans assembleur. Depuis un shell MSYS2 avec
devkitPro (`DEVKITPRO=/opt/devkitpro`) et Python 3 + Pillow :

    make            # build/deeper.gba
    make test       # tests unitaires sur PC (règles, solveurs, générateurs, moteur)
    make emutest    # scénarios joués dans mGBA (build avec --script)
    make check      # les deux
    make puzzles    # regénère data/puzzles/*.bin (graines fixes)
    make assets     # redessine les PNG provisoires
    make run        # lance dans mGBA

## Commandes

| Touche | Carte | Salle |
|---|---|---|
| Croix | choisir la salle suivante | déplacer le curseur |
| A | descendre | action principale (creuser, minerai, symbole, bloc, case suivante de la galerie) |
| B | | action secondaire (note, effacer, reculer ; cavité : bloc suivant) |
| L | | utiliser un indice |
| R | | action de famille (cavité : tourner le bloc ; un fantôme montre où il se pose) |
| SELECT | | règles de la salle et commandes ; au titre : son oui/non |
| START | descendre | menu pause (reprendre, abandonner contre une vie, sauver et quitter) |

Le jeu est en français et en anglais ; la langue est demandée à chaque
démarrage (le dernier choix est présélectionné).

Notes de conception : [docs/design.md](docs/design.md).

## Licence

MIT.
