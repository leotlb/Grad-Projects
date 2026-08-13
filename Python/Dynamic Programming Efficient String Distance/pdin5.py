def swap_cost(c1, c2):
    if c1 == c2:
        return 0
    if check_vowel(c1) == check_vowel(c2):
        return 1
    return 3


def check_vowel(c):
    return c.lower() in 'aeiou'


def string_correct(s1, s2):
    len1, len2 = len(s1), len(s2)

    #dp[i][j] = custo mínimo de transformar os i-ésimos primeiros carateres de s1 nos j-ésimos caracteres de s2
    dp = [[0] * (len2 + 1) for _ in range(len1 + 1)]

    
    operations = [[None] * (len2 + 1) for _ in range(len1 + 1)]

    #Linha e coluna zero guardando custo de remoção e inserção
    for i in range(1, len1 + 1):
        dp[i][0] = i * 2
        operations[i][0] = f"R:{s1[i-1]}"
    for j in range(1, len2 + 1):
        dp[0][j] = j * 2
        operations[0][j] = f"I:{s2[j-1]}"

    #Populando a matrix de pdin
    for i in range(1, len1 + 1):
        for j in range(1, len2 + 1):

            #Caso das letras iguais
            if s1[i-1] == s2[j-1]:
                dp[i][j] = dp[i-1][j-1]
                operations[i][j] = None

            else:
                #Custo de transformar os i caracteres de s1, exceto o último, até j de s2 mais o custo da remoção
                delete_cost = dp[i-1][j] + 2
                #Custo de transformar os i caracteres de s1 até j-1 de s2 mais o custo de inserir um carácter novo
                insert_cost = dp[i][j-1] + 2
                #Custo de tranformar os i-1 caracteres de s1 até j-1 caracteres de s2 mais o custo de uma troca
                swap_cost_val = dp[i-1][j-1] + swap_cost(s1[i-1], s2[j-1])

                #Compara o menor custo
                if delete_cost <= insert_cost and delete_cost <= swap_cost_val:
                    dp[i][j] = delete_cost
                    operations[i][j] = f"R:{s1[i-1]}"
                elif insert_cost <= delete_cost and insert_cost <= swap_cost_val:
                    dp[i][j] = insert_cost
                    operations[i][j] = f"I:{s2[j-1]}"
                else:
                    dp[i][j] = swap_cost_val
                    operations[i][j] = f"T:{s1[i-1]}-{s2[j-1]}"

    #Menor custo aparece na célula que engloba todo o tamanho de s1 e de s2
    cost = dp[len1][len2]

    #Array das operações realizadas
    ops = []

    #i e j representam o percurso, de traz pra frente, de ambas as strings. Todas as operações foram gravadas quandos ambos são 0
    i, j = len1, len2
    while i > 0 or j > 0:
        op = operations[i][j]

        #Se há uma operação, acrescenta-a
        if op is not None:
            ops.append(op)
            
        
        if i > 0 and j > 0 and s1[i-1] == s2[j-1]:  #Caso de letra iguais, pula pro carácter anterior de ambas
            i -= 1
            j -= 1
        elif op and "R:" in op:                     #Caso houve remoção, pula pro carácter anterior de s1
            i -= 1
        elif op and "I:" in op:                     #Caso houve inserção, pula pro carácter anterior de s2
            j -= 1
        else:                                       #Caso contrário, houve troca, pula pro carácter anterior de ambas
            i -= 1
            j -= 1

    #Volta a ordem normal
    ops.reverse()
    
    return cost, ops

s1 = input()
s2 = input()
cost, operations = string_correct(s1, s2)
if cost:
    print(cost)
    for op in operations:
        print(op, end='')
else:
    print("0")
    print("nada a fazer")
