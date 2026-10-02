{
  description = "nfa-emp-interpreter: interpreters (C++ and Python) for .emp NFA programs over the Mata library";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";

    # Pinned to the Mata tag this interpreter's C++/Python code is written
    # against (shared_ptr<Alphabet>, unite_nondet_with, minimize_hopcroft, ...).
    # Bump deliberately: `devel` has broken this interpreter's API before.
    tools-mata = {
      url = "github:VeriFIT/mata/v1.32.45-dev.39";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    { self, nixpkgs, flake-utils, tools-mata }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
        lib = pkgs.lib;

        mata = tools-mata.packages.${system}.mata;
        mataPy = tools-mata.packages.${system}.mataPy.overrideAttrs (old: {
          # Upstream's wheel ships as dist "libmata" while the derivation is
          # named "mata-py", which trips nixpkgs' pname/.dist-info version check.
          dontCheckPythonMetadata = true;
          # libmata.nfa.nfa and libmata.utils import these unconditionally at
          # module init, but upstream only lists them as *check* inputs.
          propagatedBuildInputs = (old.propagatedBuildInputs or [ ]) ++ (with pkgs.python3Packages; [
            tabulate
            pandas
            networkx
          ]);
        });

        # FAdo, automata-lib and cached-method have no nixpkgs packaging; build
        # them straight from their (pure-python) PyPI wheels, matching
        # pyproject.toml's pins.
        cachedMethod = pkgs.python3Packages.buildPythonPackage {
          pname = "cached-method";
          version = "0.1.0";
          format = "wheel";
          src = pkgs.fetchurl {
            url = "https://files.pythonhosted.org/packages/a3/69/f4be248608758ebad3d5954bf468fc98a822c939afbb04f1eaaecfc6be1c/cached_method-0.1.0-py3-none-any.whl";
            hash = "sha256-pxcg98q1ZRMbteQYm2ylkCY1Q14OAX0P0SYXEtjD3Aw=";
          };
          pythonImportsCheck = [ "cached_method" ];
        };

        automataLib = pkgs.python3Packages.buildPythonPackage {
          pname = "automata-lib";
          version = "9.2.0";
          format = "wheel";
          src = pkgs.fetchurl {
            url = "https://files.pythonhosted.org/packages/1a/a6/166b21b655d343d9b6b174ad6489503870fb16b295a50abd863ff281df1b/automata_lib-9.2.0-py3-none-any.whl";
            hash = "sha256-0NoJbhV8ajkKWDA5CmnzTgk3h/5Sl4eXYjXoWzk4vaU=";
          };
          propagatedBuildInputs = with pkgs.python3Packages; [
            networkx
            frozendict
            typing-extensions
            cachedMethod
          ];
          pythonImportsCheck = [ "automata.fa.dfa" "automata.fa.nfa" ];
        };

        fado = pkgs.python3Packages.buildPythonPackage {
          pname = "fado";
          version = "2.2.0";
          format = "wheel";
          src = pkgs.fetchurl {
            url = "https://files.pythonhosted.org/packages/aa/4d/515319c5a46ce309cdd29c0670c753b9564ad32aa0f4ff020573d7851710/fado-2.2.0-py3-none-any.whl";
            hash = "sha256-Tzgt9mAXwklq5ARrYi7/pYzrL4Sasn43d+sP5/V6Jf0=";
          };
          propagatedBuildInputs = [ pkgs.python3Packages.deprecation ];
          pythonImportsCheck = [ "FAdo.fa" ];
          # The wheel also declares extras (visualization, SAT solving,
          # notebooks, ...) that pyinterpret's FAdo.fa usage never touches.
          dontCheckRuntimeDeps = true;
        };

        pythonEnv = pkgs.python3.withPackages (
          ps: [
            mataPy
            automataLib
            fado
          ]
        );

        empInterpreter = pkgs.gccStdenv.mkDerivation {
          pname = "nfa-emp-interpreter";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = [ pkgs.cmake ];
          buildInputs = [ mata ];

          cmakeFlags = [ "-DCMAKE_PREFIX_PATH=${mata}" ];

          installPhase = ''
            runHook preInstall
            mkdir -p "$out/bin"
            cp src/cpp/debug-emp-interpreter "$out/bin/"
            cp src/cpp/mata-emp-interpreter "$out/bin/"
            runHook postInstall
          '';

          meta = {
            description = "C++ interpreter for .emp NFA programs, backed by Mata";
            homepage = "https://github.com/VeriFIT/nfa-program-parser";
            license = lib.licenses.mit;
            platforms = lib.platforms.unix;
            mainProgram = "mata-emp-interpreter";
          };
        };

        pyinterpret = pkgs.python3Packages.buildPythonApplication {
          pname = "pyinterpret";
          version = "1.0.0";
          pyproject = true;
          src = ./src/python;

          build-system = [ pkgs.python3Packages.setuptools ];
          dependencies = [
            mataPy
            automataLib
            fado
          ];

          # Engines for libraries other than the one under test are imported
          # eagerly, so importing the CLI requires all of them to be present.
          pythonImportsCheck = [ "pyinterpret.interpret" ];
          # mataPy's dev-tag version ("0.0.0+<rev>") never satisfies the
          # "libmata>=1.0.0" pin in pyproject.toml by PEP 440 ordering, even
          # though it provides the exact API pyinterpret needs.
          dontCheckRuntimeDeps = true;

          meta = {
            description = "Python interpreter for .emp NFA programs, comparing Mata, FAdo and automata-lib";
            homepage = "https://github.com/VeriFIT/nfa-program-parser";
            license = lib.licenses.mit;
            platforms = lib.platforms.unix;
            mainProgram = "pyinterpret";
          };
        };
      in
      {
        packages = {
          default = empInterpreter;
          emp-interpreter = empInterpreter;
          pyinterpret = pyinterpret;
        };

        apps = {
          default = {
            type = "app";
            program = "${empInterpreter}/bin/mata-emp-interpreter";
          };
          mata-interpreter = self.apps.${system}.default;
          debug-interpreter = {
            type = "app";
            program = "${empInterpreter}/bin/debug-emp-interpreter";
          };
          pyinterpret = {
            type = "app";
            program = "${pyinterpret}/bin/pyinterpret";
          };
        };

        devShells.default = pkgs.mkShell.override { stdenv = pkgs.gccStdenv; } {
          name = "nfa-emp-interpreter-dev";

          inputsFrom = [ empInterpreter pyinterpret ];

          packages = [
            pkgs.cmake
            pkgs.gdb
            mata
            pythonEnv
          ];
        };
      }
    );
}
