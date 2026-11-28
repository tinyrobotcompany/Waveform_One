#!/bin/sh
set -eu
cd "$(dirname "$0")/../backend"
export DOTNET_CLI_HOME="${DOTNET_CLI_HOME:-$PWD/.artifacts/dotnet-home}"
export NUGET_PACKAGES="${NUGET_PACKAGES:-$PWD/.artifacts/nuget}"
export DOTNET_CLI_TELEMETRY_OPTOUT=1
export DOTNET_GENERATE_ASPNET_CERTIFICATE=false
dotnet_command="${DOTNET:-dotnet}"
"$dotnet_command" restore Waveform.Certificates.slnx --locked-mode
"$dotnet_command" test Waveform.Certificates.slnx --no-restore \
  --collect:"XPlat Code Coverage" --results-directory .artifacts/test-results \
  -- RunConfiguration.TreatNoTestsAsError=true
