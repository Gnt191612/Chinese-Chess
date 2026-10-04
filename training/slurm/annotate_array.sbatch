#!/bin/bash
#SBATCH --job-name=xiangqi-label
#SBATCH --partition=short
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --time=01:00:00
#SBATCH --array=0-0
#SBATCH --output=training/logs/%A_%a.out
#SBATCH --error=training/logs/%A_%a.err

set -euo pipefail

# TODO: Copy teacher.example.json to local.json and set an absolute engine path.
# TODO: Change the array range to match the number of generated shards.
repo_dir="${SLURM_SUBMIT_DIR}"
shard_id=$(printf '%05d' "${SLURM_ARRAY_TASK_ID}")
input_file="${repo_dir}/training/data/processed/shards/part-${shard_id}.jsonl"
output_file="${repo_dir}/training/data/labels/part-${shard_id}.jsonl"
config_file="${repo_dir}/training/config/local.json"

test -f "${input_file}"
test -f "${config_file}"
mkdir -p "${repo_dir}/training/data/labels" "${repo_dir}/training/logs"

python3 "${repo_dir}/tools/training/annotate_uci.py" \
  --config "${config_file}" \
  --input "${input_file}" \
  --output "${output_file}" \
  --resume

