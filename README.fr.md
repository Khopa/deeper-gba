# Deeper

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green?style=flat-square" alt="MIT licence"></a>
  <img src="https://img.shields.io/badge/platform-Game%20Boy%20Advance-7b68ee?style=flat-square" alt="Platform: Game Boy Advance">
  <img src="https://img.shields.io/badge/language-C-00599c?style=flat-square" alt="Written in C">
  <img src="https://img.shields.io/badge/languages-FR%20EN-f5c400?style=flat-square" alt="Two languages">
  <img src="https://img.shields.io/badge/puzzles-2908-8b5cf6?style=flat-square" alt="2908 puzzles in the banks">
  <img src="https://img.shields.io/badge/AI-Claude%20Opus%205%20--%20High-d97757?style=flat-square" alt="AI: Claude Opus 5 - High">
  <a href="https://github.com/Khopa/deeper-gba/commits/main"><img src="https://img.shields.io/github/last-commit/Khopa/deeper-gba?style=flat-square" alt="Last commit"></a>
</p>

Un puzzle roguelike pour Game Boy Advance, spin-off de *Dwarves Manager*
(Android, 2013). Des nains creusent le sous-sol descente après descente : une
trentaine de salles de puzzles logiques sur une carte à embranchements, de la
surface au noyau terrestre.

*English version: [README.md](README.md).*

<table align="center">
  <tr>
    <td align="center"><img src="assets/marketing/Cover.png" width="300" alt="Deeper, la jaquette : un nain et sa pioche devant la mine"></td>
    <td align="center"><img src="docs/fullrun.gif" width="480" alt="Une descente complète, graine 20260913, jouée par le banc de test"></td>
  </tr>
  <tr>
    <td align="center"><em>La jaquette</em></td>
    <td align="center"><em>Une descente entière de 15 paliers, de la surface au noyau, enregistrée depuis mGBA (<code>make fullrun</code>)</em></td>
  </tr>
</table>

## Captures

<table align="center">
  <tr>
    <td align="center"><img src="docs/screens/dig.png" width="240" alt="Fouilles"></td>
    <td align="center"><img src="docs/screens/vein.png" width="240" alt="Filons"></td>
    <td align="center"><img src="docs/screens/ledger.png" width="240" alt="Carnet de prospection"></td>
  </tr>
  <tr>
    <td align="center"><em>Fouilles</em></td>
    <td align="center"><em>Filons</em></td>
    <td align="center"><em>Carnet de prospection</em></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/screens/tunnel.png" width="240" alt="Galerie"></td>
    <td align="center"><img src="docs/screens/block.png" width="240" alt="Cavité et son plateau de blocs"></td>
    <td align="center"><img src="docs/screens/firedamp.png" width="240" alt="Face de grisou"></td>
  </tr>
  <tr>
    <td align="center"><em>Galerie</em></td>
    <td align="center"><em>Cavité et son plateau de blocs</em></td>
    <td align="center"><em>Face de grisou</em></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/screens/core.png" width="240" alt="Le Cœur : une image dans le minerai"></td>
    <td align="center"><img src="docs/screens/fight.png" width="240" alt="Un gobelin, des séquences de touches contre sa barre d'attaque"></td>
    <td align="center"><img src="docs/screens/wall.png" width="240" alt="La paroi : marteler A avant la fin du compte"></td>
  </tr>
  <tr>
    <td align="center"><em>Le Cœur : une image dans le minerai</em></td>
    <td align="center"><em>Un gobelin, des séquences de touches contre sa barre d'attaque</em></td>
    <td align="center"><em>La paroi : marteler A avant la fin du compte</em></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/screens/map.png" width="240" alt="La carte : choisir la salle suivante"></td>
    <td align="center"><img src="docs/screens/crates.png" width="240" alt="Trois caisses, une à ouvrir"></td>
    <td align="center"><img src="docs/screens/camp.png" width="240" alt="Le marchand au campement"></td>
  </tr>
  <tr>
    <td align="center"><em>La carte : choisir la salle suivante</em></td>
    <td align="center"><em>Trois caisses, une à ouvrir</em></td>
    <td align="center"><em>Le marchand au campement</em></td>
  </tr>
</table>

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

## Comment c'est fait

| | |
|---|---|
| **Code** | [Claude Opus 5](https://claude.com) (raisonnement *High*, abonnement Claude Pro), une seule conversation de la première ligne à ce README |
| **Musique** | [Suno](https://suno.com) (licence Pro) |
| **Graphismes** | [PixelLab](https://pixellab.ai) (palier Pixel Apprentice) |

Le tout ensuite ajusté à la main : game design, équilibrage et pixel art.

## Commandes

| Touche | Carte | Salle |
|---|---|---|
| Croix | choisir la salle suivante | déplacer le curseur |
| A | descendre | action principale (creuser, minerai, symbole, bloc, case suivante de la galerie) |
| B | | action secondaire (note, effacer, reculer ; cavité : bloc suivant) |
| L | | utiliser un indice |
| R | | action de famille (cavité : tourner le bloc ; un fantôme montre où il se pose) |
| SELECT | | règles de la salle et commandes |
| START | descendre | menu pause (reprendre, abandonner contre une vie, sauver et quitter) |

Le jeu est en français et en anglais ; la langue est demandée à chaque
démarrage (le dernier choix est présélectionné), puis l'écran titre mène au
menu : Continuer, Nouvelle descente, Comptoir, Carnet, Options (son, langue).

Notes de conception : [docs/design.md](docs/design.md).

## Licence

MIT.
