import { defineConfig } from 'vite';
import { fileURLToPath } from 'node:url';

export default defineConfig({
  server: {
    // The local file dependency loads generated PDU converters lazily from
    // its sibling repository.
    fs: {
      allow: [fileURLToPath(new URL('../..', import.meta.url))]
    }
  }
});
