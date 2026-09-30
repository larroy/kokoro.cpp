## Code Style
- Limit cyclomatic complexity of functions, small well structured functions with less than 10 local
  variables are preferred
- Respect SOLID programming principles. Single responsibility, open-closed, Liskov substitution,
  Interface segregation and dependency inversion.
- Prefer functional style than imperative.
  definition.
- For variables with physical meaning such as time, suffix with an abbreviation of the unit.
  (Example `wait_s`).
- Max line length: 120 characters

### For python:

- Use `uv` for dependency management (not raw pip)
- Type hints required for function signatures
- Prefer data classes and explicit data structures vs dynamic dictionaries.
