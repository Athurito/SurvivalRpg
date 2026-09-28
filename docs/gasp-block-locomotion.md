# GASP-06: erweiterbare Block-Locomotion

Stand: 28.09.2026. **Laufkadenz und Block-Foley korrigiert und erneut geprüft; Nutzer-Sicht-/Hörprobe offen.**
[PR #160](https://github.com/Athurito/SurvivalRpg/pull/160) bleibt offen. Dieser Bericht beschreibt den Neuaufbau nach dem vollständigen Rückbau des vorherigen GASP-06-Versuchs; frühere Pose-, Netzwerk- und Cook-Ergebnisse gelten dafür nicht. Das [Manifest](assets/gasp-block-locomotion.json) enthält die aktuellen Prüfergebnisse, erhaltene Fehlversuche und Abnahmegrenzen.

## Korrektur nach Nutzerprobe: Laufkadenz und Block-Foley

Die Nutzerprobe von `bf90d218` fand zwei reale Regressionen: CMC spielte die freie Laufanimation nahezu doppelt schnell, und Mover spielte beim langsamen Blocken Rennschritte. Normales Blocktempo war laut Nutzer unauffällig. Die nachfolgenden älteren Neuaufbau-Prüfungen hatten diese Fehler nicht erfasst.

Der finale Interrupt-Blend des gemeinsamen Parents aktualisierte mit `bAlwaysUpdateChildren=true` beide Wege zum selben Basiseingang. CMC besitzt dort keinen Cache und aktualisierte die Locomotion doppelt. Movers Cache verhinderte das Doppeltempo, aber der unsichtbare Basispfad lieferte weiterhin Run-Notifies. Nur dieser finale Blend verwendet jetzt `false`; `BlockUpperBody.bAlwaysUpdateSourcePose=true` bleibt für laufende Beine unter kurzen Reaktionen erhalten. Gameplaytempo, C++ und alle Clip-Rates sind unverändert.

Die 24 abgeleiteten Walk-Starts/-Loops/-Stops erhalten **50 projektlokale Walk-L/R-Notifies** an ihren vorhandenen Kontaktmarkern. Der anfängliche Korrekturstand war noch stumm: `BS_BlockWalk.bShouldMatchSyncPhases=true` setzte über UE5.8s `ResetBlendSamples` den vorherigen und aktuellen Samplezeitpunkt gleich, sodass die Notify-Abfrage ein Zeitdelta von null erhielt. Diese Zusatzoption ist jetzt `false`; normales Marker-Sync und `HighestWeightedAnimation` bleiben aktiv. Kein Enginepatch, keine künstlich halbierte Playbackrate und keine Änderungen an Quellclips.

| Neue Prüfung | Ergebnis |
| --- | --- |
| Tatsächliche freie CMC-Bewegung, 600 cm/s | Gemessener Fußzyklus ca. 3,529 → 1,791 Hz; nach Blockrelease ca. 3,529 → 1,846 Hz |
| Block, beide Varianten bei 157 cm/s | Finale Fußzyklen ca. 0,889 Hz; je vier Walk-L- und drei Walk-R-Triggerframes, null Run-Triggerframes |
| Spielmix-Aufnahmen aus frischem Editor | Beide Block-WAVs 3,989 s mit sieben getrennten Energiegruppen, ca. 1,786/1,754 Hz; keine Mikrofonaufnahme |
| Start/Diagonal/Stop/Release | CMC 828, Mover 791 eindeutige Frames; Stop übernimmt die Kontaktphase, Release setzt Cap auf null und blendet Layer in ca. 0,194/0,198 s aus; keine Wiederaktivierung |
| Fokus auf finalen Assets | **16/16 Success**, 0 Fehler/Skips, 209,784897 s Testzeit, 623 Warnungen |
| Kaltes Neuladen und Kompilieren | 24 Clips/50 vollständige Notifyrecords, Kontaktmarker und unveränderte RateScale bestätigt; drei betroffene AnimBPs kompiliert; Editor sauber |
| Werkzeug-Negativkontrollen | Bestehender Track und doppeltes Event vor Änderung abgewiesen; vollständiger Readback und Dirty-State unverändert |
| Fünf-Karten-Cook | Exit0; 446.460 s Wandzeit, 3170 gespeicherte Pakete; Success - 0 error(s), 3 warning(s) |
| Erhaltung | Seit `bf90d218` exakt 26 geänderte Assets: Parent, BlendSpace, 24 Walkclips. Alle 35 Quellclips und sieben SaveGames unverändert; gegen Rückbaubasis weiterhin 4656 unverändert, sechs erwartete Änderungen und 49 neue Assets |

Die acht Übergangs-Nahaufnahmen wurden selbst geprüft: sichtbare Schultern/Arme bleiben verbunden, Schildhaltung und Releasezustand sind nachvollziehbar. Die Füße sind in diesen breiten Nahaufnahmen unten angeschnitten; daraus wird keine vollständige neue Fußkontakt-Sichtabnahme abgeleitet. Fußzyklusmessung ist ein Posevergleich bei tatsächlich gleicher Geschwindigkeit, keine ausgelesene Motion-Matching-Playerclock. Die WAV-Hüllkurve bestätigt Tonausgabe und Kadenz, kein eigenständiges Hörurteil über Klangqualität oder samplegenaue Audio-/Pose-Synchronität. Mover-Free-/Released-WAVs sind nur 2,709/3,115 s lang. Für den Block steigt die gemessene Frequenz durch den separaten BlendSpace-Fix gegenüber dem noch stummen Zwischenstand um rund 5,9 %; sie wird nicht als exakt unverändert ausgegeben.

Der erste neue Fokuslauf bleibt als **18/19** mit einem 45-s-Timeout im Mover-Fixed-Rollback-Test erhalten; derselbe Test besteht im abschließenden 16er-Lauf. Das ist kein behaupteter Fix eines Netzwerkfehlers. Die 623 Warnungen und der frühere stille Foley-Zwischenstand bleiben dokumentiert. Da C++ unverändert ist, wurde kein neuer nativer Build behauptet; die unten stehenden Editor-/Game-Builds sind die bereits ausgeführten Neuaufbau-Builds.

Die frische Registry-Closure der 49 neuen Assets umfasst durch Foley nun 366 Projektpakete/39 Engine-/Script-/Plugin-Grenzen und ist vollständig. Die gesamten 55 erreichen weiterhin 2127/78 und genau die bereits bekannte fehlende Soft-Previewreferenz; keine neuen Referenten und keine Original-GASP-Abhängigkeiten. Der Gesamt-Closure-Aufruf meldet deshalb weiterhin Exit1; das ist kein verdeckter grüner Gesamtgraph. Dynamische Stringloads bleiben außerhalb des Registrybeweises.

Die generischen MCP-Werkzeuge `add_animation_notifies` und `animation_notify_contract` ermöglichen explizite neue Tracks mit Undo beziehungsweise lesenden Vertragsvergleich. Sie wählen keine Spielinhalte selbst. Neue Animationssätze benötigen eigene passende Kontaktmarker und Foley-Notifies; beim Ausschalten des Blocks darf die Host-Locomotion pro Frame nur einen aktiven Updateweg erhalten.

Aktuelle Rohbelege: `Saved/GaspBlockCadence20260928` sowie `Saved/GaspBlockLayers20260927/cadence-focus-01/02-*`. Saved bleibt ignoriert und ist in anderen Checkouts nicht automatisch vorhanden. **PR #160 bleibt offen bis zur erneuten Nutzer-Sicht-/Hörprobe:** auf CMC ohne RMB laufen; auf Mover RMB halten und WASD, Diagonalen, Stoppen und Loslassen prüfen.

## Ausgangspunkt und Umfang

Implementierungscommit: `ac87a4a6a0a25f5b90633e4ed3e8ef12c987b845`. Rollback-Checkpoint: `de852e984e65009d6c68cc2be7d98cc94caa0bb5`, zurück auf die Implementierungsbasis `908b7c7544a42848afe6c8b673988735ebd4ba1b`. Die lokale Prüfung `Saved/GaspBlockLayers20260927/rollback-verification.json` dokumentiert 28 zurückgesetzte Implementierungspfade, davon acht Binärassets mit Hashes, und keinen verbleibenden Runtime-Diff zur Basis. Der zurückgenommene Stand war `20d1f7ffe62c8baff982d52ea5f64f41f6766aa2`. Historische Berichte bleiben historische Belege.

Der neue Kandidat verwendet ein vom ausgerüsteten Blockgegenstand ausgewähltes, ganzkörperliches Linked AnimBP. Ein gemeinsamer Blueprint-Parent führt den kosmetischen Ablauf aus; Datenkinder wählen ihre Animationen und Metadaten über eigene Chooser Tables. C++ besitzt Aktivierungsidentität, Geschwindigkeit, Prediction und den Lebenszyklus der Layerbindung. Es gibt keinen nativen Auswahlalgorithmus für konkrete Clips oder Phasen und keine neue konkrete native GameplayAbility.

Zielvarianten sind die vorhandenen CMC- und Mover-Pawns. Die vorhandene GAS-Blockfähigkeit, Offhand-Priorität, RPG-Equipment-Grants, PlayerState-ASC und Death-/Respawn-Abläufe bleiben kanonisch. Eine reduzierte Geschwindigkeit während des Blocks ist ausdrücklich zulässig. Animation darf weder den Capsule-Transform noch autoritative Geschwindigkeit oder Blockzustand schreiben.

## Native Verträge

| Besitzer | Vertrag |
| --- | --- |
| `FRpgWeaponBlockDefinition` in `RpgWeaponInstance.h` | Optionales `BlockLocomotionLayer` und `MovementSpeedLimit` in cm/s. Null-Layer behält den bisherigen Montage-Loop; Cap `0` erhält das gewöhnliche Geschwindigkeitslimit. Ein positives Cap erhöht das normale Limit niemals. |
| `URpgGameplayAbility_Block` | Kopiert die Definition je Aktivierung und erwirbt eine ASC-Lease mit diesem Cap. Bei konfiguriertem Layer entfällt nur der gehaltene Legacy-Loop. Optionale Start-, End-, Treffer- und Perfect-Block-Montagen bleiben GAS-eigen. |
| `URpgAbilitySystemComponent` | `BeginBlockMovement`/`EndBlockMovement` binden Lease, Ability, Avatar und ActivationPredictionKey. Cleanup verbraucht die eigene Identität vor synchronen Callbacks. Avatarwechsel und EndPlay setzen sie zurück. Der Server akzeptiert kein vom Client geliefertes Geschwindigkeitslimit. Bestätigte Block-/PerfectBlock-Reaktionen erreichen den Owner über die unten beschriebene identitätsgebundene Bridge. |
| `URpgEquipmentManagerComponent` | Bindet genau die Klasse der aktiven Blockquelle am GameplayMesh. Die Offhand-Instanz muss auf einem Client erst aufgelöst sein, bevor eine Mainhand-Ausweichbindung erlaubt ist. FastArray-Empfang, ASC-Bindung, Mesh-/AnimInstance-Wechsel und Reinitialisierung erneuern die Bindung; Unlink betrifft nur die eigene Klasse am bisherigen MainInstance. |
| CMC | SavedMoves enthalten lokale Block-/Cap-Snapshots und eine eigene unveränderliche ControlRotation für wiederholtes Replay. Der Server verwendet seine tatsächliche ASC-Lease. Rotation überschreibt CMC-Flags nur im PhysicsRotation-Aufruf; bestehende RootMotion-/Mantle-Rotation hat Vorrang. |
| Mover | ProduceInput bewahrt den rohen Bewegungs-/Sprintwunsch. Lokale Block-/Cap-Werte sind Historydaten und werden nicht als autoritative Clientbefehle serialisiert. Die wirksame Policy verändert nur die private StartingData-Kopie eines Simulationsticks; der Server sampelt seine Lease. `FRpgMoverBlockMovementSyncState` überträgt Blockbool und Cap gemeinsam an Proxies. Die native Klammer berücksichtigt sowohl `CommonLegacy.MaxSpeed` als auch vorrangige nichtnegative `Simple/SmoothWalking.MaxSpeedOverride`; nach dem Tick werden nur noch eigene installierte Werte zurückgesetzt. Authored Gaitwechsel bleiben erhalten; Beschleunigung und Velocity werden nicht künstlich überschrieben. |
| `URpgAnimInstance` | Erfasst auf dem Game Thread ausschließlich Werte: Aktivität, tatsächliche Geschwindigkeit im Actor-Bezugssystem, geglätteten GameplayMesh-Yaw für kosmetische Drehkompensation, Inputabsicht, Bodenzustand, Unterbrechung und Cap. Worker-Graphen fragen Equipment oder GAS nicht selbst ab. `EvaluateAnimationChooser` wertet das konfigurierte Table gegen diese Instanz auf dem Game Thread aus. |

Die Lease erhält den rohen Shift-Wunsch; nach Ende des Blocks kann die vorhandene Sprintpolicy wieder wirksam werden. Der aktuelle Mover hat einen getrennten Sprintgait. Aus CMC wird hier kein zusätzlicher Sprintmodus abgeleitet. Auf CMC-Simulated-Proxies bedeutet Cap `0` auch „kein Cap-Snapshot verfügbar“; Cliptempo muss aus tatsächlicher Geschwindigkeit und authored Referenztempo entstehen, nicht aus dieser Null einen unbegrenzten Block ableiten.

Der Server erzeugt Block-/PerfectBlock-Ereignisse autoritativ. Weil die normale GAS-Montagereplikation den Autonomous Owner ausnimmt, bestätigt die ASC nur diese beiden Tags per zuverlässigem Owner-RPC, gebunden an Avatar, SpecHandle und ursprünglichen ActivationPredictionKey. Der Owner verwirft fremde oder beendete Aktivierungen und gibt das bestätigte Ereignis an die bereits aktive Ability weiter; diese wählt ihre gesnapshotete Montage selbst. Es werden weder Schaden noch ein frei wählbares Montageasset übertragen. Listen-Host und Simulated Proxy erhalten keinen zweiten lokalen Bridge-Aufruf.

Bei Cancellation optiert die Block-Ability jetzt in `SuppressAbilityInputUntilRelease` ein: Nur ihr lokaler SpecHandle wird bis zum tatsächlichen Input-Release gesperrt. Wiederholtes Enhanced-Input-`Triggered` kann dadurch während noch ausstehender Stagger-Tags keine neue Aktivierung beginnen. Bereits queued Release, Avatarwechsel und Spec-Entfernung räumen den Vertrag sauber auf; ein temporäres `ClearAbilityInput` hebt ihn nicht auf. Die ASC erfindet dafür kein Release-Ereignis. Die normale Release-/Neu-Press-Folge und andere Abilities behalten ihr Verhalten. Der lokale Test `SurvivalRpg.Combat.Block.Lifecycle.CancelledHoldRequiresPhysicalRelease` und beide Netzwerk-Reaktionsfälle bestehen im abschließenden Fokuslauf.

Death cancelt bestehende GameplayAbilities über den vorhandenen Death-Pfad. CMC deaktiviert Bewegung; Mover behält seinen terminalen Dead-Zustand und verwirft dort Blockbewegung. Das Anim-Readmodel unterdrückt Block bei Tod, Ragdoll und fehlendem Bodenzustand. Eine weiterhin ausgerüstete Layer darf gebunden bleiben, muss jedoch ihre Eingabepose durchreichen. Sie darf keine Death-/Traversal-Montage ersetzen oder ihren Ablauf verzögern. Die ausgewählten Block-/Death-/Traversal-Regressionen bestehen; daraus wird keine Abdeckung jeder denkbaren Kombination abgeleitet.

## Designer- und Hostvertrag

Gemeinsame Assets:

- `/Game/SurvivalRpg/Characters/GASP/Shared/RPG/Block/ALI_RpgBlockLocomotion`: Layer `BlockLocomotion`, Eingabepose `BasePose`.
- `/Game/SurvivalRpg/Characters/GASP/Shared/RPG/Block/ABP_RpgBlockLocomotion`: abstrakter gemeinsamer Blueprint-Parent auf `URpgAnimInstance`; Auswahl und Übergänge bleiben dort sichtbar und editierbar. 59 Variablen besitzen dokumentierte Tooltips, einschließlich acht Variablen für die Fußphasenübergabe.
- `/GF_Combat_Core/Animations/BlockLocomotion/ABP_Block_SwordShield`: erster Satz.
- `/GF_Combat_Core/Animations/BlockLocomotion/ABP_Block_TestVariant`: zweites Datenkind für den Bindungs-/Wechselvertrag; kein Nachweis eines beliebigen fremden Packs oder Skeletts.
- `/GF_Combat_Core/Animations/BlockLocomotion/SwordShield/CHT_BlockLocomotion` und `BS_BlockWalk`; zweites Table unter `TestVariant/CHT_BlockLocomotion`.

Beide Hosts implementieren das Interface und reichen im ungebundenen Fallback `BasePose` durch. Die Layer ist nach den GASP-Locomotion-Korrekturen und vor dem Fullbody-Montageslot/PoseHistory integriert. Der verknüpfte Parent verwendet die Montage-Auswertungsdaten des MainInstance; GAS bleibt Montagebesitzer. Kurze Blockreaktionen verwenden den neuen Slot `BlockUpperBody`, Fullbody-Unterbrechungen weiterhin `DefaultSlot`. Der neue Slot ist die gezielte Änderung am gemeinsamen UEFN-Skelett; globale Bone-RetargetModes wurden für diesen Neuaufbau nicht geändert. Die sichtbare Qualität bleibt separat abzunehmen.

`PackChooser` ist eine `UChooserTable`-Referenz des Datenkindes. Der aktuelle Blueprint-Vertrag verwendet `Phase`, `Direction` und `TurnDebt` als Auswahlwerte und liefert Animation, Dauer, Referenztempo und signierten Turnwinkel. Die konkreten Phasen sind **Designerinhalt**, kein natives Enum:

| Phase | Aktueller Satz | Auswahlergebnis |
| --- | --- | --- |
| 0 | Idle | Block-Idle-Sequence |
| 1 | Start | Acht Richtungs-Starts |
| 2 | Loop | Ein zyklischer BlendSpace1D; dessen Direction-Parameter wird während der Bewegung aktualisiert |
| 3 | Stop | Acht Richtungs-Stops |
| 4 | Turn | Linke/rechte 90°-/180°-Sequences nach signiertem TurnDebt |

Die Loopauswahl wird nicht bei jedem Richtungswechsel als neue Clipinstanz gestartet. Die acht Loops liegen bei −180°, −135°, −90°, −45°, 0°, 45°, 90° und 135°; der Rückwärtsclip wird am +180°-Rand wiederholt. Winkelbehandlung, Tempometadaten und Kontaktmarker gehören zum jeweiligen Satz.

Für Turn verwendet der Parent einen eigenen SequenceEvaluator: ExplicitTime und die Abfrage von `BlockRootYaw` verwenden dieselbe `TurnTime`, die Entry-Rate bleibt während dieses Turns konstant, und das Kurvendelta wird einmal konsumiert. Die letzte Turn-Sequence/-Zeit bleibt während des Ausblendens erhalten. Bewegungsinput unterbricht sofort; Richtungsumkehr geht über Idle und einen authored Restart-Abstand. Diese implementierten Zeit- und Übergangsverträge sind noch keine visuelle Qualitätsbestätigung, insbesondere bei raschem Loslassen/Wiederdrücken.

## Erster Animationssatz und Retarget-Grenze

Der Hauptsatz umfasst **35 abgeleitete Sequences**: je acht Walk-Starts/-Loops/-Stops, vier Turns sowie sieben Idle-/Block-/Parry-Sequences. Die vier fehlerhaften frühen Pilot-Sequences unter `TestVariant/Sequences` wurden entfernt; das zweite Datenkind verwendet die korrigierten Hauptsatz-Clips. Quelle ist das projektlokale `Sword_and_Shield`-Pack, Ziel das vorhandene UEFN-Gameplay-Skelett. Die 35 Quellpakete wurden für diesen Bericht gegen die LFS-Payload-Hashes des Rollback-HEAD geprüft: **35/35 identisch**. Das ersetzt keinen abschließenden Preservation-Check aller übrigen Projektassets und SaveGames.

Der Retarget-Aufruf benennt Quell-/Zielmesh, Zielrig, Pelvis und Root ausdrücklich. Der automatisch gewählte Ziel-Root `pelvis` war für diesen Satz ungeeignet; im **neu erzeugten Retargeter** lautet das Mapping `root → root`, Pelvis bleibt `pelvis → pelvis`. Das vorhandene Zielrig und Quellassets werden dadurch nicht umgeschrieben. Die FK-Chain-Translation bleibt `None`; explizite Toe-/Foot-Chain-Aliase und ausgeschlossene Fuß-/Ball-Bones gehören zu diesem Satz. IK Solve ist im abgeleiteten Retargeter deaktiviert. Autoalignment ist keine Fußkontaktabnahme.

Nach der korrigierten Retarget-Ausgabe beträgt das gemessene natürliche Looptempo 157,432629 bis 174,308151 cm/s (horizontale Netto-Rootstrecke geteilt durch Dauer). Der aktuelle Satz zielt auf **157 cm/s**. `author_pack.py` setzt je Loop `RateScale = 157 / gemessenes Tempo`; die allgemeine Blueprint-Playbackrate berücksichtigt anschließend reale Bewegung gegenüber dem Satzreferenztempo. Die frühere 162-cm/s-Vorüberlegung ist nicht das aktuelle Tuning dieses Satzes.

Die Normalisierung arbeitet nur an markierten abgeleiteten Sequences: `BlockRootYaw` speichert Root-Yaw in Grad relativ zum ersten Frame, unwrapped und vor Root Lock abgetastet; `ForceRootLock=true`, `RootMotionRootLock=AnimFirstFrame`, `EnableRootMotion=false`. Es wird keine zusätzliche Bewegungsautorität aus dem Clip aktiviert. `FootContacts` enthält explizite `Left`-/`Right`-Marker. Die Normalisierungsantworten melden historisch `saved=false`: der spätere Save, die abschließende Assetkompilierung und der Cook sind separat belegt.

Die Kontaktanalyse verwendet 32 komprimierte Zielpose-Samples, Fuß- und Ballhöhe sowie Geschwindigkeit und zusammenhängende Stützintervalle. Alle acht Loops erfüllen den dokumentierten Kandidatengate. Das beweist weder reale Kollision noch Foot Lock. Besonders zu erhalten sind diese Grenzen:

- Alle acht Stops beginnen mit rechter Stütze; beliebiger nahtloser Stopzeitpunkt ist nicht belegt.
- Der rechte Kontakt von `B_L45` verschiebt sich zwischen nomineller und strengerer Schwelle deutlich; vor endgültigem Tuning visuell prüfen.
- Start-/Stop-/Turnmarker können Settling oder Pivot statt neuem Auftreten markieren. Idle, Hit und Parry erhalten keine erfundenen Stützmarker.
- Cleanup von Block, Trefferunterbrechung oder Tod wartet niemals auf ein Kontaktfenster.

Der implementierte Loop→Stop-Übergang liest einmal vor dem Phasen-/Chooserwechsel die zuletzt fertig ausgewertete `BlockFeet`-Markerposition des MainInstance. Zwei Chooseroutputs geben den ersten linken (`L`) und folgenden rechten (`R`) Kontakt des gewählten Stops an. Bei `Right→Left` beginnt der Stop bei `alpha*L`, bei `Left→Right` bei `L+alpha*(R−L)`. Erforderlich sind `0<L<R<Dauer` und Alpha in [0,1]; sonst sowie bei Start→Stop bleibt der explizite Einstieg 0. `PhaseElapsed` beginnt am gewählten Einstieg, sodass nur die verbleibende Clipdauer läuft. Es gibt weder wiederholtes Seeking noch Gameplay-Verzögerung.

Vier reale Standalone-Aufnahmen (CMC/Mover, jeweils beide Markerhälften) zeigen genau einen stabilen Einstieg, **vier von vier korrekte Mappings ohne Fehler**, keine Rückwärtsschritte des Phasentimers und den Wechsel zu Idle am Clipende innerhalb des beobachteten Frameintervalls. Die vier Standbilder wurden geprüft. Das belegt diese beiden Strafe-Richtungen und den Controller-Zeitvertrag; weder BlendStack-interne Zeit, Welt-Footlock, identische Übergangsposen noch Multiplayer-Phasenparität werden behauptet. Späte Einstiege überspringen Teile des Bremsclips; die Nutzer-Sichtabnahme bleibt maßgeblich.

## Weiteres Pack anschließen

1. Quellclips, Skelett, Richtungen, Root-/Pelvis-Konvention, Root Motion, Notifies und vorhandene Kontaktvarianten inventarisieren. Quellpakete und globale Bone-RetargetModes gemeinsam verwendeter Skelette nicht zur Anpassung eines einzelnen Packs verändern.
2. In einem neuen Packordner abgeleitete Zielclips erzeugen. Die versionierten Werkzeuge `Build/Tools/Unreal/animation_retarget_tools.py` und `animation_asset_tools.py` verlangen explizite Pfade und Zuordnungen; neue Ergebnisse werden nicht still überschrieben oder gespeichert. Das erste Pack ist keine generische Retarget-Voreinstellung.
3. Arm-/Beintranslationen, Ziel-Pelvishöhe, Rootmapping und komprimierte Pose prüfen. Anschließend natürliche Tempi und Kontaktintervalle messen, daraus passende Cap-/Playbackdaten wählen und nur diese Ableitungen normalisieren.
4. Auf dem **kompatiblen Zielskelett** ein Datenkind des gemeinsamen Parents, eigenen Chooser und eigenen Richtungs-BlendSpace konfigurieren. Alle verwendeten Phasen müssen Assets und gültige Metadaten liefern. Optionale GAS-Reaktionsmontagen und Slots separat prüfen.
5. `BlockLocomotionLayer` und `MovementSpeedLimit` der vorhandenen WeaponInstance-Definition zuweisen; die vorhandene EquipmentDefinition bleibt für Grants und Offhand-Auswahl zuständig. Keine Runtime-Konfigurationsreplikation aus dem lokalen Testsetter ableiten: gewöhnliche Clients laden dieselben authored Definitionen über ihre replizierten Equipmentinstanzen.
6. Native Assetvalidierung, positive und gezielt negative Chooserfälle, Hostbindung, Release/Reentry, Equipwechsel/Reinitialisierung, Owner/Authority/Proxy/LateJoin, Korrektur und Death prüfen. Dann speichern, frisch laden, Abhängigkeiten und Preservation prüfen, cooken und die sichtbaren Übergänge abnehmen lassen.

**Kein Plug-and-play für beliebige Skelette:** `IsAnimationCompatible` verlangt die exakte TargetSkeleton-Identität des kompilierten AnimBP. Ein anderes Skelett braucht eine ausdrücklich passende Retarget-/AnimBP-/Host-Konfiguration und eigene Prüfung von Proportionen, Bones, Slots, Kurven und Notifies. Der optionale Runtime-Retarget-Follower bleibt kosmetischer Verbraucher des GameplayMesh und übernimmt weder Equipment- noch Physics-/RootMotion-Autorität.

## Frühere Neuaufbau-Validierung vor der Nutzerfehlermeldung

| Abschließende Prüfung | Tatsächliches Ergebnis |
| --- | --- |
| Editor11 | Exit0; 79,06 s Unreal, 79,40 s Wandzeit |
| Game02 | Exit0; 81,17 s Unreal, 82,56 s Wandzeit |
| Focus04 | **19/19 Success**, 0 Fehler/Skips, 218,444519 s, 618 Warnungen, 0 Divide-by-zero |
| Regression02 | **25/25 Success**, 0 Fehler/Skips, 144,446350 s, 482 Warnungen |
| Finaler Cook | Exit0; 166,06 s Commandlet, 240,379990 s Wandzeit; `NumPackagesSaved=3170`, 0 Fehler, 3 Warnungen |
| Assetkompilierung | Neun Blueprints/Interface mit `warnings_as_errors` erfolgreich; danach keine Dirty-Assets/-Maps |
| Preservation02 nach Cook/letzter Test-PIE | 4.662 Baselineassets: 4.656 unverändert, exakt sechs erwartete Änderungen; 49 neue, keine entfernten/unerwarteten; sieben SaveGames unverändert |
| Statische Prüfungen | Python-Syntax gültig, vier generierte Plugin-Overrides verifiziert, unabhängiger Source-/Vertragsreview ohne neuen Must-fix-Befund |

Die Warnungen bleiben sichtbar: Fokus enthält 551 PIE-NetGUID-, 58 Voice-/Interface-, acht NP-Equal-Pending-Frame- und eine PoseSearch-AsyncIndex-Warnung. Regression enthält 378 NetGUID-, 82 Voice-/Interface-, sieben NP-, 14 bestehende Manny-PoseAsset-/Quellmismatch-Warnungen sowie die vorhandene Respawn-Widget-Tickwarnung. Der Cook meldet GameplayCue-Pfadsuche, MCP-EULA-Hinweis und die Respawn-Widget-Tickwarnung. Die erfolgreichen Läufe werden nicht als warnungsfrei ausgegeben; `3170` ist die gespeicherte Paketstatistik, keine Behauptung über 3170 ausschließlich neu gekochte Assets.

Die fünf Schema-/Packvalidierungstests erreichen die vorhandene WeaponDefinition und beide konkreten Datenkinder. Der designer-eigene Hook läuft auf einer uninitialisierten transienten Kopie mit SkeletalMesh-Outer; er prüft 22 Auswahlfälle, Skelettidentität, Dauer, Tempo, Turnwinkel/-kurve und Stopkontakte. Gezielte fehlende Chooser/Pflichtresultate bleiben negative Kontrollen. Der Fokus umfasst außerdem unveränderlichen CMC-Replay-Yaw, stale Reaction-Identitäten, Cancel bis zum echten Release, Cap-Snapshot/Cap0, Kameraausrichtung, beide Mover-Sprint-Reihenfolgen, gehaltenes Shift nach Release, LateJoin, Fixed-Korrektur, Authority-Cancel sowie Layerwechsel und AnimInstance-Reinitialisierung. Der Profilwechsel verändert nur transiente Instanzen aller Rollen; er belegt keine Runtime-Replikation geänderter Definitionstuningdaten.

Die Netzwerk-Reaktionstests senden echte autoritative GAS-Block-/PerfectBlock-Ereignisse und beobachten die Montage auf Authority, Owner und Late Observer. Sie behaupten keinen neuen Damage-/PerfectWindow-Berechnungsnachweis. **Die vorhandene globale Stagger-Ability besitzt standardmäßig keine Montage.** Nur die gespawnte Authority-Instanz erhält im Test vorübergehend die kompatible Shield-GuardBreak-Montage im `DefaultSlot`; der vorherige Wert wird exakt restauriert. Stagger bleibt die vorhandene `ServerOnly`-Ability: Fullbody-Montagepriorität wird auf Authority/Simulated Proxy geprüft, während der Owner Tag und Block-Cleanup erhält. Eine neue Owner-Übertragung der Stagger-Montage wird nicht behauptet.

Eine lokale Turnprobe belegte einen echten Präsentationsfehler: Actor-Yaw-Kompensation kämpfte gegen Mover-Meshglättung. Der Snapshot folgt nun Mesh-Yaw, während die Richtungswahl im Actor-Bezugssystem bleibt. Im gleichen 8-s-Mouse-Look-Muster sinken Welt-Root-Rückschritte >0,1° nach 0,7 s von **218/816 auf 0/823**, in Idle von 51/120 auf 0/113. Die Turn-/Stopbilder wurden selbst geprüft; diese Messung belegt den lokalen Fehler und dessen Korrektur, keine allgemeine Netzwerk- oder Nutzer-Sichtabnahme. Die abschließenden CMC-/Mover-Reaktionsaufnahmen sind ebenfalls abgeschlossen: In vier echten lokalen Block-/PerfectBlock-Montagefenstern bleiben 157 cm/s und LayerAlpha 1 erhalten, während Oberschenkel/Waden weiter rotieren. Alle 978 Samples enthalten den aktiven Blockzustand; die fünf geprüften Hit-/Parry-Bilder zeigen zusammenhängende Gliedmaßen. Das sind lokale autoritative GAS-Ereignisse, keine neue Damage-/PerfectWindow- oder Netzwerkabnahme. Der erste Mover-Aufnahmeversuch scheiterte vor Eventdispatch am Python-Klassenalias und bleibt als Werkzeugfehler erhalten; die korrigierte Aufnahme änderte keinen Runtimecode.

Die 49 neuen Assets besitzen eine vollständig aufgelöste Registry-Closure mit 84 Projektpaketen und 34 expliziten Engine-/Script-/Plugin-Grenzen; keine fehlenden Dateien/Records, unbekannten Mounts oder alten GASP-Importreferenzen. Die gesamten 55 (49 neu + sechs geändert) erreichen 2.127 Projektpakete/78 Grenzen, enthalten aber weiterhin **eine historische fehlende Soft-Previewreferenz** auf `/Game/Sword_and_Shield/Demo/Mannequins/Meshes/SKM_Manny_Simple` über eine vorhandene ShieldCombo. Alle 371 direkten Softreferenten sind gegenüber der historischen Registry unverändert, keiner ist neu; die neuen 49 erreichen dieses Ziel nicht. Die Gesamt-Closure wird daher nicht pauschal als vollständig behauptet. Dynamische Stringloads und interne Engine-/Plugin-Closure sind nicht aus Registrydaten bewiesen.

Die sechs vorgesehenen Assetänderungen betreffen beide Host-AnimBPs, Mover-Pawnpolicy, UEFN-Slot, Shield-Definition und Block-Ability-Tags. Der Shared-Parent ist abstrakt, beide Kinder konkret; vier fehlerhafte Pilotclips wurden entfernt. Der aktuelle Bestand umfasst 49 neue Assets und 59 dokumentierte Blueprintvariablen.

## Erhaltene Fehlversuche

| Historischer Versuch | Ergebnis und Einordnung |
| --- | --- |
| Editor05 / Editor07 | Exit6: zunächst falscher Testhelper-Basistyp, später Syntaxfehler im in Bearbeitung befindlichen Reaktionstest. Editor06/08/09/10/11 bestehen. |
| Focus01 | **7/13**, 23.660 Divide-by-zero-Meldungen; fehlende Mover-Mode-Override-Klammer und veraltete Kind-Blendzeiten0. |
| Focus02 | **12/18**, 611 Warnungen, 0 Divide-by-zero. Native Fixture ohne lokalen Controller, zu früher stationärer Mover-Testvorlauf und nicht konfigurierte globale Stagger-Montage. |
| Focus03 | **17/18**, 619 Warnungen, 0 Divide-by-zero. Mover-Staggerfall beobachtete zwei Aktivierungen/Enden; lokales Cancel-Input-Latch verhindert Wiederaktivierung vor dem echten Release. |
| Regression01 | **23/25**, 485 Warnungen, 147,280899 s. Mantle-Terminalfall mit Engine-Ensure `bHasPriorBaseInfo` (`BasedMovementUtils.cpp:497`); Vault-Terminalfall mit Hintergrund-MCP-Fehler `resources/templates/list`. |

Der Mover-Testvorlauf wird jetzt vor jeder Messung einmal aus echten Authority-Beschleunigungs-/Glättungswerten und Ausgangsgeschwindigkeit berechnet. Die anschließenden 0,75s-Min/Max-Grenzen bleiben unverändert; es gibt kein Warten bis zu einer grünen Probe. Regression02 auf getrenntem MCP-Port8002 enthält keinen erneuten Engine-Ensure. Dies ist ein erfolgreicher Wiederholungslauf, **kein behaupteter Fix des historischen Engine-Ensure**. Frühere Ergebnisse des vollständig zurückgebauten GASP-06-Versuchs werden nicht übertragen.

## Sichttest und Übergabe

`/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle` (CMC) oder `Lvl_RpgGaspMover` starten. RMB halten, in alle WASD-Richtungen gehen, anhalten und die Kamera bei gehaltenem RMB drehen. Rasches Loslassen/Wiederdrücken sowie beim Mover Shift vor/während Block und weiter gehaltenes Shift nach Release prüfen. Die reduzierte Blockgeschwindigkeit ist beabsichtigt. Sichtbar zu beurteilen sind Haltung, Schulter-/Schildanschluss, Schritte und Start-/Stop-/Turnübergänge.

**Nutzer-Sichtabnahme bleibt offen; PR #160 bleibt Draft und wird nicht gemergt.** Die vorhandene PR-Beschreibung bezeichnet noch einen historischen Stand: Aktualisierung von Titel/Body ist derzeit durch Connector 403 und den abgemeldeten Browser blockiert. Der aktuelle Ersatztext liegt lokal unter `Saved/GaspBlockLayers20260927/pr-160-block-locomotion-metadata.md`; das betrifft die Metadatenpflege, keinen zusätzlichen Freigabeschritt. Der letzte Testeditor wurde ohne Dirty-Assets/-Maps geschlossen. Für die Nutzerprobe steht ein normal sichtbarer Editor auf der Mover-Karte bereit, noch ohne PIE. Die Umsetzung ist in `ac87a4a6` gespeichert; der Branch bleibt derselbe offene Draft-PR.

Rohbelege unter `Saved/GaspBlockLayers20260927/` sind ignoriert. Das Manifest hält Pfade und Hashes fest; andere Checkouts benötigen den gespeicherten Content, verifizierte Plugin-Overrides und eigene Ausführungsbelege. Bestehende NET03-/VAL-Grenzen bleiben bestehen.
