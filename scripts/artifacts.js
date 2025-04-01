const fs = require('fs');
const path = require('path');

// Get the app name from env
const appName = process.env.APP_NAME || 'shared-memory-store';

// Create directory for npm packages if it doesn't exist
if (!fs.existsSync('./npm')) {
  fs.mkdirSync('./npm', { recursive: true });
}

// Get all artifacts
const artifactsDir = path.join(__dirname, '..', 'artifacts');
const artifacts = fs.readdirSync(artifactsDir);

// Move each artifact to npm directory
for (const artifact of artifacts) {
  const artifactDir = path.join(artifactsDir, artifact);
  const files = fs.readdirSync(artifactDir);
  
  for (const file of files) {
    if (file.startsWith(appName) && file.endsWith('.node')) {
      const source = path.join(artifactDir, file);
      const dest = path.join(__dirname, '..', 'npm', file);
      fs.copyFileSync(source, dest);
      console.log(`Moved ${file} to npm directory`);
    }
  }
}
