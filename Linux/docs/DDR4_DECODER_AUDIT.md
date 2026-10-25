# Audit DDR4 avant extension

Audit source du 25 octobre 2026, base TuxTimings
`1167bd1e729792eb220ee05b515bb43dff34ff43` et copie locale ZenStates-Core propre
à `869b7e82572337e5a673e92230edc92bddf0e44b`. Les fichiers
`Dictionaries/DDR4Dictionary.cs`, `Hardware/DRAM/Ddr4Timings.cs` et
`Hardware/DRAM/BaseDramTimings.cs` ont été consultés. Cette référence
communautaire n'est pas une spécification AMD et ne certifie pas les autres
familles. La passation comparait encore TuxTimings `ffd12bf` ; ses instructions
de réapplication ne s'appliquent pas au dépôt actuel.

## Changements historiques déjà présents

Dans `Linux/src/dram.c`, tRFC4 est extrait de [31:22], FGR de `0x5012C[18:16]`,
les codes fixes/on-the-fly/réservés sont distingués et le chemin DDR4 convertit
les cycles par `2000 / débit_MT_s`, sans diviser à nouveau sous 2000 MT/s.
Le contrôle I/O existant couvre le refresh DDR4. Ces apports ne sont pas à
réimplémenter. La capture historique Vermeer reste 630/468/288 cycles et
350/260/160 ns à DDR4-3600 ; elle n'est pas une preuve concernant les bits qui
valent zéro dans cette capture.

La règle RFC0/RFC1 existante reste conservée, y compris ses cas zéro/défaut.
Elle ne prouve pas une correspondance avec un canal ou rang peuplé. Les FGR 5/6
n'identifient pas le refresh actuellement émis ; le champ tRFC de base n'est pas
renommé en timing actif.

## Différences encore ouvertes à la base

Intervalles inclusifs. La colonne TuxTimings décrit le code, sans garantir que
tous les bits ont une sémantique DDR4 sur chaque génération.

| Champ | Registre | TuxTimings | ZenStates DDR4 consulté | Décision |
|---|---|---|---|---|
| Ratio | `0x50200` | [7:0] / 3 × 200 MT/s | `GetBits(offset=0,n=7)` = [6:0] / 3 | Ne pas supprimer le bit 7 sans preuve discriminante de famille/layout. |
| FAW | `0x50210` | [7:0] | [6:0] | Recherche ; test futur bit 7 isolé, puis capture autorisée. |
| WR | `0x50218` | [7:0] | [6:0] | Idem ; examiner séparément la substitution si zéro. |
| RDRDSCL | `0x50220` | [29:24] | [27:24] | Recherche DDR4 ; ne pas rétrécir WRWRSCL par symétrie. |
| RDWR | `0x50228` | [13:8] | [12:8] | Recherche, bit 13 discriminant. |
| STAG | `0x50250` | [26:16] | [23:16] | Recherche, bits 26:24 discriminants. |
| STAGsb | `0x50250` | [8:0], visible DDR4 aussi | Absent du dictionnaire DDR4 | Sémantique DDR4 non démontrée ; ne pas annoncer un nouveau support. |
| PHYRDL | `0x50258` | [23:16] | [21:16] | Recherche, bits 23:22 discriminants. |
| PHYWRL | `0x50258` | [15:8] | [12:8] | Recherche, bits 15:13 discriminants. |
| WRPRE | `0x502A4` | Code brut [10:8] | Getter brut + 1 | Code et durée ne doivent pas être confondus ; pas de nouvelle formule sans périmètre établi. |
| RDPRE | `0x502A4` | Code brut [2:0] | Getter brut < 2 ? brut + 1 : brut | Même réserve ; cas 0,1,2 et frontière à couvrir séparément. |

Les principaux champs CL/RCDRD/RCDWR/RAS/RP/RC, RRDS/RRDL/RTP, CWL/WTRS/WTRL,
TRCPAGE, RDRDSC/SD/DD, WRWRSCL/SC/SD/DD, WRRD, MOD/MODPDA/MRD/MRDPDA, CKE/XP,
PHYWRD ainsi que Cmd2T[10] et GDM[11] correspondent à la référence consultée.
Cette concordance de code n'est pas une validation matérielle.

## Substitutions à qualifier

Le chemin commun initial remplace RCDWR nul par RCDRD et WR nul par WTRS.
ZenStates ne réalise pas ces substitutions dans le décodeur consulté. Ce sont
des dérivations héritées et non des valeurs directement lues. Un statut de
lecture distinct empêche de substituer après une panne mais ne justifie pas la
formule elle-même. Toute conservation doit indiquer la source dérivée et ses
dépendances ; une suppression future doit être DDR4/DDR5 délibérée et testée.
Un zéro reçu avec succès ne prouve pas que le champ n'existe pas.

## Résultat de recherche et prochaine preuve utile

Aucun masque ni getter nouveau n'est adopté sur la seule concordance
communautaire. Le helper étant partagé avec DDR5, une correction globale
modifierait un autre layout sans preuve. BankGroupEn, DLLK/XS et les autres
champs power-down restent différés : leur présence dans un dictionnaire ne
suffit pas à établir une couverture de toutes les plateformes AMD.

La prochaine étape utile consiste à sélectionner une seule règle, une famille
et un layout établis, puis réunir une référence de registre ou une capture
qualifiée qui distingue les interprétations. Ajouter alors cas de bit frontière,
zéro/réservé, positif et non-application DDR5 avant la correction ciblée.
Aucune capture, lecture matérielle, transaction SMN, compilation ou installation
n'a été effectuée pour cet audit documentaire.

Références de code consultées localement :

- [Dictionnaire DDR4 versionné](https://github.com/irusanov/ZenStates-Core/blob/869b7e82572337e5a673e92230edc92bddf0e44b/Dictionaries/DDR4Dictionary.cs).
- [Décodeur DDR4 versionné](https://github.com/irusanov/ZenStates-Core/blob/869b7e82572337e5a673e92230edc92bddf0e44b/Hardware/DRAM/Ddr4Timings.cs).
- [Classe commune versionnée](https://github.com/irusanov/ZenStates-Core/blob/869b7e82572337e5a673e92230edc92bddf0e44b/Hardware/DRAM/BaseDramTimings.cs).
