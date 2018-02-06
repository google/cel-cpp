# CEL Evaluator

This is the top level directory for C++ - based implementation of CEL ( Common
Expression Language - go/api-expr ) Evaluator. The purpose of the component is
to evaluate AST trees resulting from parsing of CEL expressions.

## Directory Structure

*   public - External library interfaces, classes intended for caller use
*   eval - Implementation of expression evaluator

## Syncing with Gob

Preliminary: for conveniency you can set up this alias.

```sh
alias copybara='/google/data/ro/teams/copybara/copybara'
```

### Importing from Gob

```sh
copybara third_party/java_src/my_project/copy.bara.sky
```

### Exporting to Gob the first time

To be on the safe side you might want to test first with
`--git-destination-skip-push`


```sh
copybara third_party/cel/cpp/copy.bara.sky piper_to_empty_gob --init-history --force
```

### Exporting to Gob

You can only export from a submitted CL.

To be on the safe side you might want to test first with
`--git-destination-skip-push`

```sh
export CL=11111111
copybara third_party/cel/cpp/copy.bara.sky piper_to_gob ${CL} --git-destination-push=cl-${CL} --git-destination-fetch=cl-${CL}
```
(where 11111111 is a submitted CL)

### Testing export

```sh
copybara third_party/cel/cpp/copy.bara.sky piper_to_folder 11111111
```
(where 11111111 is a CL, that could be pending.)
