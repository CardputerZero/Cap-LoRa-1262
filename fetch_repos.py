import json
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent


def load_repos():
    with (ROOT / "repos.json").open(encoding="utf-8") as file:
        return json.load(file)


def repo_path(repo):
    return ROOT / repo["path"]


def is_repo_fetched(repo):
    path = repo_path(repo)
    return path.is_dir() and (path / ".git").is_dir()


def git_output(path, *args):
    return subprocess.run(
        ["git", "-C", str(path), *args],
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    ).stdout.strip()


def validate_repo(repo):
    path = repo_path(repo)
    origin = git_output(path, "remote", "get-url", "origin")
    if origin != repo["url"]:
        raise RuntimeError(f"Unexpected origin for {repo['path']}: {origin}")
    status = git_output(path, "status", "--porcelain")
    if status:
        raise RuntimeError(f"Dependency has local changes: {repo['path']}")
    head = git_output(path, "rev-parse", "HEAD")
    if head != repo["revision"]:
        raise RuntimeError(
            f"Unexpected revision for {repo['path']}: {head}; "
            f"expected {repo['revision']}"
        )


def fetch_repo(repo, update_existing=True):
    path = repo_path(repo)
    if not (path / ".git").is_dir():
        subprocess.run(["git", "clone", repo["url"], str(path)], check=True)
    else:
        origin = git_output(path, "remote", "get-url", "origin")
        if origin != repo["url"]:
            raise RuntimeError(f"Unexpected origin for {repo['path']}: {origin}")
        if git_output(path, "status", "--porcelain"):
            raise RuntimeError(f"Dependency has local changes: {repo['path']}")
        if update_existing:
            subprocess.run(
                ["git", "-C", str(path), "fetch", "--tags", "--prune"], check=True
            )

    subprocess.run(
        ["git", "-C", str(path), "checkout", "--detach", repo["revision"]],
        check=True,
    )
    validate_repo(repo)


def fetch_dependencies(repos=None, update_existing=True):
    for repo in repos or load_repos():
        fetch_repo(repo, update_existing=update_existing)


def ensure_dependencies():
    for repo in load_repos():
        if is_repo_fetched(repo):
            validate_repo(repo)
            continue
        print(f"Cap-LoRa-1262: fetching missing dependency: {repo['path']}")
        fetch_repo(repo, update_existing=False)


if __name__ == "__main__":
    fetch_dependencies()
