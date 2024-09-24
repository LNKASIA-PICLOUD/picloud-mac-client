import os
import platform
import builtins
from tempfile import gettempdir
from configparser import ConfigParser


def read_env_file():
    envs = {}
    script_path = os.path.dirname(os.path.realpath(__file__))
    env_path = os.path.abspath(os.path.join(script_path, '..', '..', '..', 'envs.txt'))
    with open(env_path, 'rt', encoding='UTF-8') as f:
        for line in f:
            if not line.strip():
                continue
            if line.startswith('#'):
                continue
            key, value = line.split('=', 1)
            envs[key] = value.strip()
    return envs


def get_config_from_env_file(env):
    envs = read_env_file()
    if env in envs:
        return envs[env]
    raise KeyError(f'Environment "{env}" not found in envs.txt')


def isWindows():
    return platform.system() == 'Windows'


def isLinux():
    return platform.system() == 'Linux'


def getWinUserHome():
    return os.environ.get('UserProfile')


def getClientRootPath():
    if isWindows():
        return os.path.join(getWinUserHome(), 'owncloudtest')
    return os.path.join(gettempdir(), 'owncloudtest')


def getConfigHome():
    if isWindows():
        # There is no way to set custom config path in windows
        # TODO: set to different path if option is available
        return os.path.join(getWinUserHome(), 'AppData', 'Roaming', 'ownCloud')
    return os.path.join(get_config_from_env_file('XDG_CONFIG_HOME'), 'ownCloud')


def get_default_home_dir():
    if isWindows():
        return getWinUserHome()
    return os.environ.get('HOME')


# map environment variables to config keys
CONFIG_ENV_MAP = {
    'localBackendUrl': 'BACKEND_HOST',
    'secureLocalBackendUrl': 'SECURE_BACKEND_HOST',
    'maxSyncTimeout': 'MAX_SYNC_TIMEOUT',
    'minSyncTimeout': 'MIN_SYNC_TIMEOUT',
    'lowestSyncTimeout': 'LOWEST_SYNC_TIMEOUT',
    'middlewareUrl': 'MIDDLEWARE_URL',
    'clientLogFile': 'CLIENT_LOG_FILE',
    'clientRootSyncPath': 'CLIENT_ROOT_SYNC_PATH',
    'tempFolderPath': 'TEMP_FOLDER_PATH',
    'clientConfigDir': 'CLIENT_CONFIG_DIR',
    'guiTestReportDir': 'GUI_TEST_REPORT_DIR',
    'ocis': 'OCIS',
    'screenRecordOnFailure': 'SCREEN_RECORD_ON_FAILURE',
}

DEFAULT_PATH_CONFIG = {
    'custom_lib': os.path.abspath('../shared/scripts/custom_lib'),
    'home_dir': get_default_home_dir(),
}

# default config values
CONFIG = {
    'localBackendUrl': 'https://localhost:9200/',
    'secureLocalBackendUrl': 'https://localhost:9200/',
    'maxSyncTimeout': 60,
    'minSyncTimeout': 5,
    'lowestSyncTimeout': 1,
    'middlewareUrl': 'http://localhost:3000/',
    'clientLogFile': '-',
    'clientRootSyncPath': getClientRootPath(),
    'tempFolderPath': os.path.join(getClientRootPath(), 'temp'),
    'clientConfigDir': getConfigHome(),
    'guiTestReportDir': os.path.abspath('../reports'),
    'ocis': False,
    'screenRecordOnFailure': False,
}
CONFIG.update(DEFAULT_PATH_CONFIG)

READONLY_CONFIG = list(CONFIG_ENV_MAP.keys()) + list(DEFAULT_PATH_CONFIG.keys())


def read_cfg_file(cfg_path):
    cfg = ConfigParser()
    if cfg.read(cfg_path):
        for key, _ in CONFIG.items():
            if key in CONFIG_ENV_MAP:
                if value := cfg.get('DEFAULT', CONFIG_ENV_MAP[key]):
                    if key in ('ocis', 'screenRecordOnFailure'):
                        CONFIG[key] = value == 'true'
                    else:
                        CONFIG[key] = value


def init_config():
    # try reading configs from config.ini
    try:
        script_path = os.path.dirname(os.path.realpath(__file__))
        cfg_path = os.path.abspath(
            os.path.join(script_path, '..', '..', '..', 'config.ini')
        )
        read_cfg_file(cfg_path)
    except:
        pass

    # read and override configs from environment variables
    for key, value in CONFIG_ENV_MAP.items():
        if os.environ.get(value):
            if key in ('ocis', 'screenRecordOnFailure'):
                CONFIG[key] = os.environ.get(value) == 'true'
            else:
                CONFIG[key] = os.environ.get(value)

    # Set the default values if empty
    for key, value in CONFIG.items():
        if key in ('maxSyncTimeout', 'minSyncTimeout'):
            CONFIG[key] = builtins.int(value)
        elif key in ('localBackendUrl', 'middlewareUrl', 'secureLocalBackendUrl'):
            # make sure there is always one trailing slash
            CONFIG[key] = value.rstrip('/') + '/'
        elif key in (
            'clientRootSyncPath',
            'tempFolderPath',
            'clientConfigDir',
            'guiTestReportDir',
        ):
            # make sure there is always one trailing slash
            if isWindows():
                value = value.replace('/', '\\')
                CONFIG[key] = value.rstrip('\\') + '\\'
            else:
                CONFIG[key] = value.rstrip('/') + '/'


def get_config(key=None):
    if key:
        return CONFIG[key]
    return CONFIG


def set_config(key, value):
    if key in READONLY_CONFIG:
        raise KeyError(f'Cannot set read-only config: {key}')
    CONFIG[key] = value


def clear_scenario_config():
    global CONFIG
    initial_config = {}
    for key in READONLY_CONFIG:
        initial_config[key] = CONFIG[key]

    CONFIG = initial_config
