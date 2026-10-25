# Transport SMN et portée UMC

Analyse du 25 octobre 2026 à partir du dépôt local `ryzen_smu`, HEAD propre
`d2983668300dd2a598e5a7dc40e71ce0678cc270`, `drv.c` blob
`9dd747186dd618a0dd3df8fe59ac8dc7c7fbee9c`, version déclarée `0.1.7`.
Cette identité concerne les sources examinées ; la version réellement installée
ou chargée sur l'hôte n'a pas été vérifiée. Aucun appel au pilote n'a été lancé.

## Contrat observé

Dans `drv.c`, `smn_store()` choisit l'opération selon sa longueur :

- quatre octets, adresse little-endian : demande de lecture d'un registre ;
- huit octets, adresse puis valeur : écriture d'un réglage cible, interdite dans
  ce projet ;
- autre longueur : retourne zéro.

`Linux/src/dram.c` ne soumet que quatre octets. Il vérifie ouverture, nombre
exact soumis, flush, seek, nombre exact reçu et close. Aucun retry de soumission
partielle n'est ajouté. Écrire l'adresse dans sysfs demande néanmoins une
transaction PCI/SMN au pilote ; cela n'est pas une lecture de fichier passive.

La réponse se trouve dans `g_driver.smn_result`, commune à tous les clients.
`smn_show()` copie simplement ce mot. L'échec de `smu_read_address()` est
journalisé mais `smn_store()` retourne quand même le nombre d'octets soumis.
La réponse ne contient ni adresse, ni numéro de demande, ni état matériel.
Dans `smu.c`, `amd_pci_mutex` sérialise bien la sélection d'adresse PCI et la
lecture/écriture des données d'une opération. Il ne couvre pas la paire
`smn_store()` → `smn_show()` de chaque processus.

## Traces logiques et limites

Les traces suivantes sont des conséquences de lecture du code, pas des
simulations exécutées ni des observations sur le matériel.

| Situation | Déroulement autorisé par l'interface | Conclusion |
|---|---|---|
| Deux clients intercalés | A soumet adresse X ; B soumet Y ; A lit le résultat global | A peut recevoir la valeur de Y tout en réussissant toutes ses I/O. |
| Échec de lecture PCI | Un résultat précédent est présent ; la sélection/lecture échoue ; `smn_store` retourne quatre | Le client ne peut pas identifier l'échec matériel à partir du compte d'octets ; valeur antérieure possible. |
| Canal inactif renvoyant zéro | Soumission réussie ; zéro retourné | Impossible de distinguer canal inactif et zéro valide par ce seul résultat. |
| Deux canaux différents | Lecture UMC0 puis une adresse d'un autre canal, sans topologie ni identité retournée | Des valeurs différentes n'établissent pas leur correspondance avec les emplacements DIMM. |
| Version incompatible | Même chemin sysfs mais sémantique ou format modifié | La présence du fichier ne prouve pas que le protocole versionné ci-dessus s'applique. |
| Topologie ambiguë | Série SMBIOS absente/dupliquée ou canal non établi | Aucune association par ordre, offset voisin ou valeur plausible. |

Un mutex dans TuxTimings ou un verrou de fichier conventionnel ne contraint pas
les autres consommateurs. Répéter une lecture et obtenir la même valeur ne
prouve pas sa fraîcheur ou sa correspondance. Une lecture avec statut utilisateur
reste utile pour éviter les faux zéros après erreur, avec cette limite exposée.

## Décision pour TuxTimings

Les timings actuels viennent d'UMC0 (`offset = 0`) pour les deux routes DRAM.
Le sélecteur de barrette change ses informations et sa température, sans changer
le contrôleur lu. L'interface doit le dire explicitement. Le décodage d'une
adresse reconnue reste distinct du support matériel validé d'une plateforme.

Conserver le transport quatre octets existant pour ce lot et exposer les erreurs
utilisateur ; ne pas annoncer une garantie de fraîcheur, de transaction atomique
multi-processus ou des timings par DIMM. Aucun changement du dépôt externe
`ryzen_smu` n'est inclus.

Un futur protocole pilote devrait fournir au minimum, dans une transaction liée
au client : version ABI, adresse demandée, valeur, état matériel et identifiant de
réponse. Il devrait corréler demande et réponse sans état global non attribué,
borner les tailles, refuser les formats incompatibles et ne donner aucune
capacité d'écriture de réglage au chemin de lecture de TuxTimings. Ce sont des
exigences de conception, pas une ABI livrée ni une autorisation de modifier le
pilote.

## Découverte des canaux : conditions de reprise

Aucune découverte générale de canaux actifs ni correspondance UMC/rang/DIMM n'est
établie par les sources auditées. Avant d'étendre la lecture :

1. établir famille, capacités, nombre et offsets de canaux à partir d'une source
   versionnée adaptée ; ne pas transposer les offsets DDR4 au DDR5 ;
2. définir comment l'interface distingue canal absent, erreur et zéro valide ;
3. prouver une topologie ou une identité reliant le contrôleur et le DIMM, si
   l'affichage doit prétendre à des timings par barrette ;
4. couvrir avec un faux transport les accès intercalés, la réponse périmée, la
   version incompatible, les canaux inactifs/différents et la topologie ambiguë ;
5. prévoir une validation matérielle séparée et explicitement autorisée.

Les contrôles logiciels actuels ne peuvent pas fabriquer l'information que le
pilote ne retourne pas. Le résultat de ce lot de recherche est donc un contrat
limité et des prérequis, sans nouveau balayage matériel ni offset supposé.

Sources consultées localement : `drv.c` (`smn_show`, `smn_store`,
`g_driver.smn_result`), `smu.c` (`smu_smn_rw_address`, `smu_read_address`) et
`README.md`, section `/sys/kernel/ryzen_smu_drv/smn`, à la révision indiquée.
[Source du pilote versionnée](https://github.com/amkillam/ryzen_smu/blob/d2983668300dd2a598e5a7dc40e71ce0678cc270/drv.c).
