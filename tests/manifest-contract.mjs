import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
const root = path.resolve(import.meta.dirname, '..');
const sdkRoot = process.env.MW_SCHEMA_ROOT || path.resolve(root, '../MyWallpaper/packages/sdk-public');
const { parseAddonManifest, getSchemaDefaultsForScope, isValidSettingValue } = await import(pathToFileURL(path.join(sdkRoot, 'dist/addon-schema/index.js')));
const manifest = parseAddonManifest(JSON.parse(fs.readFileSync(path.join(root, 'manifest.json'), 'utf8')));
const defaults = getSchemaDefaultsForScope(manifest, 'device');
if (Object.keys(defaults).length !== 61 || manifest.settings.length !== 69) throw new Error('Unexpected settings count');
for (const name of fs.readdirSync(path.join(root, 'presets'))) {
  const values = JSON.parse(fs.readFileSync(path.join(root, 'presets', name), 'utf8'));
  for (const setting of manifest.settings) if (setting.type !== 'section' && !isValidSettingValue(setting, values[setting.id])) throw new Error(`${name}: ${setting.id}`);
}
console.log('Current MyWallpaper SDK accepted 69 definitions, 61 device values and all six preset snapshots.');
