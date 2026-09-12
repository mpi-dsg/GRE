#!/bin/bash
set -uo pipefail
cd /home/GRE_Longitudinal
echo "==== LIBIO RID $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
./experiments/16_rid_longitudinal_libio_st.sh || true
echo "==== PLANET RID $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
./experiments/16_rid_longitudinal_planet_st.sh || true
echo "==== ALL RID DONE $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
