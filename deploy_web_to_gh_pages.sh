#!/usr/bin/env bash
set -euo pipefail

# Deploy the contents of the web/ folder to the gh-pages branch and push to origin.
# Usage: ./deploy_web_to_gh_pages.sh

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
cd "$ROOT_DIR"

if [ ! -d web ]; then
  echo "No se encontró la carpeta 'web'"
  exit 1
fi

# Create a one-off branch containing only the contents of web/
git subtree split --prefix=web -b gh-pages-deploy

# Push that branch to origin as gh-pages
git push -u origin gh-pages-deploy:gh-pages --force

# Remove the temporary branch locally
git branch -D gh-pages-deploy

echo "Despliegue completado. La página estará disponible en https://<tu-usuario>.github.io/$(basename $(git rev-parse --show-toplevel))/ (puede tardar unos segundos en activarse)."
