#!/bin/bash
set -uo pipefail
cd /home/GRE_Longitudinal
echo "==== LIBIO RIUD nomem $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
./experiments/15_riud_nomem_libio_st.sh || true
echo "==== PLANET RIUD nomem $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
./experiments/15_riud_nomem_planet_st.sh || true
echo "==== ALL RIUD nomem DONE $(date -u +%Y-%m-%dT%H:%M:%SZ) ===="
