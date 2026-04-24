# esphome-espnow

Composants ESPHome externes permettant à deux ESP32 de communiquer via **ESP-NOW** en fallback lorsque Home Assistant ou le WiFi est indisponible.

## Scénario

```
Mode normal  : [bouton] ──WiFi──► HA ──WiFi──► [ampoule]
Mode fallback: [bouton] ──────── ESP-NOW ──────► [ampoule]
```

Le basculement est automatique : dès que le WiFi **ou** l'API HA est absent depuis `fallback_timeout` secondes, le module bouton passe en mode ESP-NOW. Il revient en mode normal après `recovery_timeout` secondes de connectivité stable.

## Structure du projet

```
esphome-espnow/
├── components/
│   ├── espnow_sender/      # module côté bouton
│   │   ├── __init__.py
│   │   ├── espnow_sender.h
│   │   └── espnow_sender.cpp
│   └── espnow_receiver/    # module côté ampoule
│       ├── __init__.py
│       ├── espnow_receiver.h
│       └── espnow_receiver.cpp
└── test/
    ├── button_device.yaml
    ├── light_device.yaml
    └── secrets.yaml
```

## Prérequis

- ESP32 (framework **esp-idf** uniquement)
- ESPHome ≥ 2026.4 installé dans un venv :
  ```bash
  python3 -m venv .venv
  .venv/bin/pip install esphome
  ```

## Installation

Référencer les composants depuis ton YAML via `external_components` :

```yaml
external_components:
  - source:
      type: local
      path: /path/to/esphome-espnow/components
    components: [espnow_sender]   # ou espnow_receiver
```

## Configuration

### Module bouton — `espnow_sender`

```yaml
espnow_sender:
  id: espnow_tx
  peer_mac: "AA:BB:CC:DD:EE:FF"   # MAC WiFi du module ampoule (requis)
  fallback_timeout: 30s            # délai avant entrée en fallback (défaut: 30s)
  recovery_timeout: 10s            # délai avant retour en mode normal (défaut: 10s)
  action: toggle                   # toggle | on | off | mirror_state (défaut: toggle)
  binary_sensor_id: my_button      # optionnel — câblage automatique des changements d'état
```

#### Paramètres

| Paramètre | Type | Défaut | Description |
|---|---|---|---|
| `peer_mac` | string | — | Adresse MAC du receiver (requis) |
| `fallback_timeout` | durée | `30s` | Temps sans connectivité avant de passer en ESP-NOW |
| `recovery_timeout` | durée | `10s` | Temps de connectivité stable avant de quitter ESP-NOW |
| `action` | enum | `toggle` | Action envoyée à l'ampoule |
| `binary_sensor_id` | id | — | Binary sensor dont les changements d'état déclenchent automatiquement un envoi |

#### Actions disponibles

| Valeur | Comportement |
|---|---|
| `toggle` | Inverse l'état de l'ampoule |
| `on` | Allume l'ampoule |
| `off` | Éteint l'ampoule |
| `mirror_state` | Reflète l'état du binary sensor (ON→ON, OFF→OFF) |

#### Déclenchement depuis YAML

En complément (ou à la place) de `binary_sensor_id`, tu peux appeler `send_command()` depuis une lambda, par exemple sur `on_press` d'un bouton :

```yaml
on_press:
  then:
    - lambda: id(espnow_tx).send_command(true);
```

`send_command()` est un no-op si le composant est en mode normal — HA garde la main.

---

### Module ampoule — `espnow_receiver`

```yaml
espnow_receiver:
  id: espnow_rx
  peer_mac: "11:22:33:44:55:66"   # MAC WiFi du module bouton (filtrage, requis)
  light_id: my_light               # LightState à contrôler (requis)
```

Le receiver est **toujours en écoute** et applique les commandes reçues. Il ne gère pas de notion de mode — c'est le sender qui décide quand envoyer.

## Mise en service

### 1. Trouver les adresses MAC

Flashe les deux ESP et lis les logs :
```
[I][wifi:xxx]: WiFi MAC Address: AA:BB:CC:DD:EE:FF
```

### 2. Mettre à jour les YAML

- `test/button_device.yaml` → `light_mac: "XX:XX:XX:XX:XX:XX"`
- `test/light_device.yaml` → `button_mac: "XX:XX:XX:XX:XX:XX"`
- `test/secrets.yaml` → credentials WiFi / API / OTA réels

### 3. Compiler et flasher

```bash
# Compiler
.venv/bin/esphome compile test/button_device.yaml
.venv/bin/esphome compile test/light_device.yaml

# Flasher via USB
.venv/bin/esphome run test/button_device.yaml
.venv/bin/esphome run test/light_device.yaml
```

## Format du paquet ESP-NOW

Trame de 2 octets :

| Octet | Valeur | Description |
|---|---|---|
| 0 | `0xE5` | Magic (filtre les paquets étrangers) |
| 1 | `0x00` | Action : OFF |
| 1 | `0x01` | Action : ON |
| 1 | `0x02` | Action : TOGGLE |

## Notes techniques

- **Canal WiFi** : `peer.channel = 0` — les deux ESP se connectent au même AP, donc même canal automatiquement.
- **Thread-safety** : le callback de réception ESP-NOW s'exécute dans la tâche WiFi. Les paquets sont transmis à la boucle principale via une FreeRTOS queue (taille 8).
- **Hystérésis** : la machine d'état passe par `PENDING_FALLBACK` et `PENDING_RECOVERY` pour éviter des oscillations rapides.
- **Receiver passif** : l'ampoule ne change pas de comportement en mode normal vs fallback — elle se contente d'appliquer toute commande ESP-NOW reçue. HA peut toujours contrôler la lumière en parallèle.
- **ESP-IDF 5.5.4** : la signature du send callback a changé (`wifi_tx_info_t` / champ `des_addr` au lieu de `const uint8_t*`).
