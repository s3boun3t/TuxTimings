# Provenance des valeurs et validation reproductible

Audit du 25 octobre 2026, base TuxTimings `1167bd1e729792eb220ee05b515bb43dff34ff43`.
Les tableaux conservent les constats de la base initiale et les limites des
sources ; ils ne constituent pas un relevé matériel. La consolidation locale
réalisée ensuite est décrite ci-dessous. Le compte rendu des commandes réellement
exécutées reste dans le suivi local.

## Contrat commun

Une valeur s'interprète avec sa source, son unité, son état de disponibilité et
sa portée. Une lecture réussie de zéro diffère d'un échec. La disponibilité des
I/O utilisateur ne prouve ni la fraîcheur du matériel ni l'identité du composant.
Une configuration SMBIOS ou contrôleur n'est pas une mesure électrique courante.
Le codename choisit une route de décodage ; il ne détecte pas le type de chaque
module. Le sélecteur DIMM ne change pas le contrôleur UMC0 lu pour les timings.

## Inventaire visible

| Famille / champs GTK | Source et transformation | Nature, unité, portée et limites |
|---|---|---|
| CPU, codename, version SMU / PM | `backend.c`, SMBIOS `dmidecode -t processor`, attributs `ryzen_smu_drv` | Identification rapportée ; codename inconnu possible. Une version ne prouve pas la compatibilité de toutes les tailles. |
| Carte, BIOS, date, AGESA | SMBIOS ; recherche AGESA dans blobs BIOS/ACPI/DMI | Identification, pas mesure. Une chaîne absente reste inconnue ; la lecture AGESA peut employer `/dev/mem`. Le harness ne doit pas appeler ce chemin. |
| Capacité, constructeur, référence, série, rang du DIMM | Type 17 SMBIOS, inventaire borné, cache de processus | Configuration rapportée. Capacité convertie en GiB ; série de remplissage écartée. Le rang absent suit encore le défaut SR du parseur initial : ce défaut ne constitue pas une preuve de rang. |
| Type mémoire global | Types SMBIOS par module ; route DRAM conservée séparément | DDR4, DDR5, LPDDR4, LPDDR5, inconnu ; un inventaire incomplet ou contradictoire ne permet pas de certifier un type unique. Le défaut initial `mem_type_for_codename()` n'identifiait DDR5 que pour 23. |
| Speed | Ratio SMN `0x50200` ; repli SMBIOS configuré ou PM MCLK | Débit en MT/s ; ratio décodé avec référence 100 MHz implicite. MCLK est une horloge en MHz : facteur deux uniquement pour les types DDR dont la relation est établie. À la base, le repli non-DDR4 perdait ce facteur et l'origine était invisible. |
| MCLK, FCLK, UCLK | Floats de la PM table sélectionnés par version, sans conversion | Horloges rapportées en MHz ; pas une preuve d'exactitude électrique. Pas de certification à partir d'une simple valeur plausible. |
| BCLK | DMI Type 4 `External Clock` en priorité ; sinon fréquence cpufreq / multiplicateur P0 MSR | MHz ; la première source est une configuration firmware entière, la seconde une estimation. Le nom `BCLK` seul masque cette distinction à la base. |
| GDM, Cmd2T, PowerDown | SMN `0x50200` et `0x5012C` ; bits différents DDR4/DDR5 | Configuration booléenne ou 1T/2T ; faux et zéro sont valides uniquement si leur registre a été lu. À la base, certaines erreurs étaient affichées comme `False` ou `1T`. |
| Timings communs | `dram.c`, registres `0x50204` à `0x502A4`, tableau ci-dessous | Champs de configuration en cycles / codes ; route commune DDR4/DDR5 mais interprétation pas universellement certifiée. Disponibilité nécessaire par registre, pas un drapeau unique pour toute la mémoire. |
| tRFC, tRFC2, tRFC4, tRFCsb | Sélection RFC propre au layout, masques `dram.c` | Cycles programmés ; la sélection reste une heuristique, sans preuve de rang ou canal. tRFC4/FGR DDR4 et tRFCsb DDR5 sont spécifiques à la route. |
| tRFC ns, tRFC4 ns, tREFI ns | Cycles × 2000 / débit DDR en MT/s | Valeurs dérivées, pas durée mesurée. Requièrent à la fois la source timing et le débit utilisables ; ne pas les calculer avec une fréquence SMBIOS de substitution sans l'expliciter. |
| FGR | `0x5012C[18:16]` DDR4 | Code configuré : 0 normal ; 1/2 fixe 2×/4× ; 5/6 on-the-fly ; 3/4/7 réservés. Ne donne pas le mode de refresh instantané ; tRFC reste le timing de base. |
| VSOC, VCORE, CLDO VDDP, VDDG CCD/IOD, VDD MISC | PM table, carte par version dans `pm_table.c` | V ; correspondance communautaire versionnée, pas sonde électrique indépendante. Pour `0x380905` et 372 floats seulement, CPU/SoC télémétrie 40/45 remplace les consignes 39/44 ; pas de repli sur ces consignes si la télémétrie échoue. Voir `PM_TABLE_INDICES.md`. |
| MEM VDD, MEM VDDQ, MEM VPP, CPU VDDIO | Attributs du module `aod_voltages`, u32 mV de région firmware / 1000 ; fenêtre utilisateur 500 < mV < 3000 | V de table firmware, sans preuve générale de mesure courante. Aucun VDIMM DDR4 ne peut être déduit de VCORE/VSOC. Zéro/échec initial donne `—` ; aucune conversion choisie pour retrouver une tension attendue. |
| PPT | Float PM selon version, fenêtre 0,5–400 | W de télémétrie interprétée. À la base, absent est stocké zéro puis affiché `0.0W`. Ne pas confondre avec une preuve de puissance nulle. `package_power_w` est distinct et non affiché. |
| VID, tensions par cœur | PM table, indices versionnés, filtrage de zéros hors chemin Vermeer huit slots | V ; VID est une demande/consigne, pas VCORE mesuré. Les numéros de cœur hors chemin huit slots peuvent être compactés ; ordre de tableau ne prouve pas l'identité physique. |
| Tdie/Tctl/Tccd1/Tccd2 | Overlay hwmon k10temp ; zenpower par libellé en repli | Entrées millidegrés / 1000 en °C. À la base, Tdie peut être copié de Tctl : le libellé n'établit pas une sonde distincte. Les indices k10temp sont présumés par ce backend ; zéro initial écarté par son helper. |
| IOD et températures par cœur | PM table et overlay hwmon `Core N` | °C, garde version/layout requise. Index 11 explicitement écarté pour Vermeer `0x380905`. Une plage plausible ne suffit pas à prouver l'index. La température CPU de repli scanne encore des candidats PM ; ce n'est pas une règle universelle. |
| RAM sensors / Temperature du DIMM | JC42/ee1004 ou SPD5118 déjà exposés, identité SPD + SMBIOS + topologie | °C, conversion millidegrés, vrai zéro accepté. Inventaire global séparé de l'association. Erreur efface l'échantillon précédent. Série unique, référence, type et parent périphérique requis ; aucun lien par ordre. Voir `DIMM_TEMPERATURES.md`. |
| Utilisation par cœur | Différences `/proc/stat`, agrégation des paires logiques | % dérivé ; première observation zéro faute de delta. La paire `2c/2c+1` ne démontre pas toutes les topologies SMT. |
| Fréquence par cœur | cpufreq, kHz / 1000, moyenne des paires logiques | MHz rapportés ; même limite de topologie. Le résumé GTK peut afficher zéro pour une composante absente lorsqu'une autre rend la ligne visible. |
| Ventilateurs / pompe | hwmon Nuvoton/nct6, `fanN_input` | RPM entiers ; premier contrôleur avec valeurs positives. Zéro initial est ignoré : ventilateur arrêté et erreur restent indiscernables. Numéro 7 étiqueté Pump par convention, sans preuve de câblage. |
| Bench latence / débit | `bench.c`, parcours aléatoire / transferts temporisés ; éventuellement module tuxbench | ns et MB/s mesurés dans le protocole du benchmark ; ne mesurent pas les timings SMN. Les tests GTK doivent substituer ces fonctions et interdire leur appel. |
| Bench Pi | `pi_bench.c`, horloge monotone, calcul GMP multithread | Temps écoulé s/ms selon calcul demandé ; pas une télémétrie matériel. Le harness ne l'exécute pas. |

### Dépendances DRAM communes

Les intervalles sont inclusifs et décrivent le code audité, sans certifier leur
validité matérielle sur toutes les routes.

| Registre UMC0 | Champs affichés / bits |
|---|---|
| `0x50204` | CL[5:0], RAS[14:8], RCDRD[21:16], RCDWR[29:24] |
| `0x50208` | RC[7:0], RP[21:16] |
| `0x5020C` | RRDS[4:0], RRDL[12:8], RTP[28:24] |
| `0x50210` | FAW[7:0] |
| `0x50214` | CWL[5:0], WTRS[12:8], WTRL[22:16] |
| `0x50218` | WR[7:0] |
| `0x5021C` | TRCPAGE[31:20] |
| `0x50220` | RDRDDD[3:0], RDRDSD[11:8], RDRDSC[19:16], RDRDSCL[29:24] |
| `0x50224` | WRWRDD[3:0], WRWRSD[11:8], WRWRSC[19:16], WRWRSCL[29:24] |
| `0x50228` | WRRD[3:0], RDWR[13:8] |
| `0x50230` | REFI[15:0] et conversion tREFI ns |
| `0x50234` | MRD[5:0], MOD[13:8], MRDPDA[21:16], MODPDA[29:24] |
| `0x50250` | STAGsb[8:0], STAG[26:16] |
| `0x50254` | XP[5:0], CKE[28:24] |
| `0x50258` | PHYWRL[15:8], PHYRDL[23:16], PHYWRD[26:24] |
| `0x502A4` | RDPRE[2:0], WRPRE[10:8] ; codes bruts dans la base |

Les substitutions RCDWR zéro → RCDRD et WR zéro → WTRS sont des valeurs dérivées
héritées, pas de nouvelles lectures. Leur origine doit rester explicite et leur
disponibilité dépend de toutes les sources effectivement utilisées.

## Matrice de preuves et cas reproductibles

Cette matrice fixe le périmètre à vérifier ; elle ne remplace pas les résultats
de commandes. `test_dram_refresh.c` et `test_pm_vermeer.c` utilisent le vrai
décodeur avec entrées synthétiques ; `test_backend_memory.c` injecte les sorties
SMBIOS. `test_dimm_sensors.c` crée une arborescence temporaire. `test_ui.c`
substitue le backend et les benchmarks, sans élévation.

| Profil / événement | Résultat attendu et preuve |
|---|---|
| DDR4 Vermeer (12), Matisse (4), routes APU historiques | Même politique I/O ; capture qualifiée 630/468/288 cycles à DDR4-3600 → 350/260/160 ns conservée. Seul ce cas a une capture historique ; les autres profils restent synthétiques. |
| DDR5 Raphael/Phoenix/Strix/Granite/Hawk (20–24) | Route DDR5, pas tRFC4/FGR DDR4 ; registre absent ne devient ni zéro actuel ni 1T. Fixtures synthétiques, aucune capture matérielle DDR5 nouvelle. |
| Codename inconnu | Pas de lecture/décodage DRAM ; type physique ne doit pas être inventé. |
| LPDDR4 / LPDDR5 SMBIOS | Type distinct conservé ; ne certifie pas la validité du layout DRAM sur LPDDR. |
| Inventaire vide, type manquant, erreur de commande, débordement, contradiction | Type global inconnu/partiel/contradictoire explicite ; pas d'association DIMM par ordre. |
| SMN absent, SMBIOS seul, PM MCLK seul | Provenance distincte ; conversion MHz → MT/s explicite et bornée au type connu. Fréquence nulle indisponible, booléen faux lu disponible. |
| Ouverture, écriture quatre octets courte, flush, seek, lecture courte, close en erreur | Chaque valeur dépendante indisponible ; aucune nouvelle soumission partielle ni retry. Les registres indépendants restent visibles. |
| Succès → échec → récupération | Aucune valeur ancienne présentée actuelle ; récupération sans redémarrage du décodeur. |
| PM inconnue, taille courte, index non mappé, garde Vermeer taille/topologie | Pas de nombre issu d'un layout supposé ni d'un index négatif ; exceptions de layout testées en application et non-application. |
| Température zéro, erreur, capteur disparu, identité absente/dupliquée | Zéro valide distinct de `Unavailable`/`Unassociated`, inventaire global conservé. |
| GTK onze scénarios historiques, fenêtre étroite, HiDPI, textes longs | Valeurs et masquages cohérents, menus lisibles. Sans display exploitable : essai non réalisé, jamais réussite visuelle. Captures synthétiques marquées avec scénario et identité du build. |
| Transport concurrent / réponse périmée / canaux inconnus | Limites prouvées par lecture du protocole, voir `SMN_TRANSPORT.md` ; non résolubles par un seul drapeau d'I/O utilisateur. |

Les captures persistantes sont volontaires ; le harness nettoie ses temporaires
par défaut. Conserver avec les images le scénario, l'identité du build et les
conditions de rendu. L'absence de matériel DDR5/LPDDR et d'affichage utilisable
limite la validation ; elle ne démontre ni panne matérielle ni support universel.

## Défauts révélés à traiter séparément

À la base auditée, `pm_table.c` choisit une carte Granite Ridge pour une version
inconnue et accepte des débuts per-core `-1` dans certaines gardes. Ce sont des
défauts distincts du décodage DRAM : ajouter des régressions de version inconnue,
Hawk Point sans index per-core et table courte avant correction. Les conventions
BCLK/Tdie, rang absent, ventilateur zéro et topologie CPU ci-dessus restent des
limites documentées tant qu'un sous-lot ne les corrige pas avec des preuves.

Les tables communautaires existantes sont des références de décodage ; seule une
capture autorisée sur une configuration identifiée peut établir la valeur brute
matérielle. Ni une suite C verte, ni la compilation GTK, ni un paquet extrait ne
valident les niveaux suivants (installation, session réelle, matériel).

## Consolidation réalisée après cet audit

Les statuts DRAM `available` (`dram_source_mask_t`) conservent le résultat I/O de
chaque registre. `dram_has()` exige toutes les dépendances et un layout connu.
Le zéro d'un registre lu reste affichable, y compris `False` et `1T` ; une erreur
produit `—`. La substitution WR depuis WTRS exige 50218 et 50214 ; RCDWR depuis
RCDRD utilise 50204. Les infobulles les identifient comme dérivations héritées à
l'interprétation non vérifiée. La sélection RFC n'a pas changé. DDR5 exige les
quatre lectures de chaque groupe RFC/RFCsb avant de l'annoncer disponible, sans
nouveau repli en cas d'échec. Les conversions ns exigent timing et ratio UMC0
utilisables, avec `2000 / data_rate_mts` pour les deux routes ; la division
heuristique DDR5 sous 2000 MT/s a été retirée (fixture 1000 MT/s, 480 cycles =
960 ns). Ce cas synthétique ne revendique pas un mode DDR5 matériel validé.

`memory_config_resolve()` sépare consensus SMBIOS, layout du décodeur et source
du débit. Sans inventaire complet concordant, le type global reste inconnu
(raison d'incomplétude ou de contradiction). LPDDR reste distinct. Le débit est
choisi parmi : ratio UMC0 compatible, configuration SMBIOS explicitement en MT/s
et concordante, puis MCLK PM × 2 seulement pour DDR4/DDR5 établis. Un inventaire
contradictoire ne donne aucun débit global. Une unité SMBIOS `MHz` ancienne est
conservée mais non doublée arbitrairement ; le champ peut employer une ancienne
sémantique de sortie, sans preuve que le nombre représente MCLK. SMBIOS rapporte
ce que déclare le firmware, sans sonder le matériel
([projet dmidecode](https://www.nongnu.org/dmidecode/)). GTK indique `(UMC0)`,
`(configured)` ou `(MCLK × 2)`, avec source en infobulle. Un codename seul ne
certifie aucun type physique.

Les versions PM inconnues ne prennent plus de layout Granite par défaut. Les
offsets de cœur négatifs sont rejetés. `ppt_available` distingue un zéro fini
lu d'une donnée absente ou invalide ; les tests couvrent 0, limites 0–400 W,
NaN/infini, taille et récupération. Ces gardes ne prouvent pas de nouveaux
layouts. Les horloges PM absentes ne s'affichent plus comme zéro MHz. BCLK garde la source
choisie : configuration SMBIOS ou estimation cpufreq/multiplicateur MSR, indiquée
dans la cellule et l'infobulle. Aucune lecture BCLK réelle n'a été faite pour ces
tests de provenance ; le scénario GTK `configured` fournit les métadonnées.

Les autres limites de l'inventaire (rang absent assimilé SR, topologie SMT,
indices hwmon présumés, disponibilités des ventilateurs et composantes par
cœur) restent ouvertes. Elles ne sont pas corrigées ni validées matériellement
par ce lot. La prochaine correction doit cibler une règle étayée et sa fixture.

## Rejouer les vérifications sans matériel

Depuis la racine du dépôt :

```sh
make -B -C Linux/tests check
make -B -C Linux PROFILE=portable all
./Linux/tuxtimings --version
# Avec un affichage GTK accessible, uniquement backend et benchmarks simulés :
UI_CAPTURE_DIR=/chemin/choisi make -C Linux/tests check-ui
```

`check` couvre DRAM, PM, identité/températures DIMM, parsing SMBIOS, politique
mémoire, lecteur de commandes, AML et sélection des instructions de benchmark,
plus livraison, protocole WMI et diagnostic DDR4 synthétiques. Il ne lance ni
le vrai backend, ni benchmark, ni installation. Les fixtures de livraison
construisent et extraient un paquet Debian dans un répertoire temporaire, et
mettent à jour exclusivement des dépôts Git temporaires avec distant local.

`check-ui` compile un exécutable sans `main.c` ; le vrai backend est remplacé,
les benchmarks font échouer le test s'ils sont appelés. Les onze scénarios
historiques sont conservés et complétés par erreur/récupération, zéro valide,
LPDDR5, configuration SMBIOS, fenêtre étroite, libellés longs et HiDPI. La
capture attend un vrai nœud rendu, avec attente bornée. Par défaut, captures et
exécutable temporaires sont nettoyés, y compris en erreur. `UI_CAPTURE_DIR`
crée un nouveau sous-dossier unique contenant PNG et manifeste (révision,
identité embarquée, profil, GTK, backend demandé et scénarios). Les fichiers
existants ne sont pas remplacés. Un échec d'affichage signifie validation UI
non réalisée ; une réussite synthétique ne remplace pas un essai matériel.
