"""This tool iterates over the dataset, converting the riegeli files to JSON files.

Example usage:

  convert_riegeli_to_json /path/to/dataset/folder /path/to/output/folder
"""

import io
import sys
import os
import json
import riegeli
import contest_problem_pb2


def write_problem_as_json(p, fp):
  """Write the given problem to a JSON file
  
  Args:
    p: problem
    fp: handle to the file
  """
  solutions = [{'language': s.language,'solution': s.solution} for s in p.solutions]
  incorrect_solutions = [{'language': s.language,'solution': s.solution} for s in p.incorrect_solutions]
  public_tests =  [{'input': t.input,'output': t.output} for t in p.public_tests]
  private_tests =  [{'input': t.input,'output': t.output} for t in p.private_tests]
  generated_tests =  [{'input': t.input,'output': t.output} for t in p.generated_tests]
  problem_dict = {
    'source': p.source,
    'name': p.name,
    'description': p.description,
    'difficulty': p.difficulty,
    'solutions': solutions,
    'incorrect_solutions': incorrect_solutions,
    'public_tests': public_tests,
    'private_tests': private_tests,
    'generated_tests': generated_tests,
    'time_limit': str(p.time_limit),
    'memory_limit_bytes': p.memory_limit_bytes,
  }
  if len(p.cf_tags) > 0:
      problem_dict['cf_tags'] = [tag for tag in p.cf_tags]
  if p.HasField('cf_contest_id'):
    problem_dict['cf_contest_id'] = p.cf_contest_id
  if p.HasField('cf_index'):
    problem_dict['cf_index']= p.cf_index
  if p.HasField('cf_points'):
    problem_dict['cf_points'] = p.cf_points
  if p.HasField('cf_rating'):
    problem_dict['cf_rating'] = p.cf_rating
  if p.is_description_translated:
    problem_dict['is_description_translated'] = True
    problem_dict['untranslated_description'] = p.untranslated_description
  else:
    problem_dict['is_description_translated'] = False
  if p.HasField('input_file'):
    problem_dict['input_file'] = p.input_file
  if p.HasField('output_file'):
    problem_dict['output_file'] = p.output_file
  json.dump(problem_dict, fp)

def convert_file_to_json(src_file, dest_file):
  """Converts the provided file to JSON
  
  Args:
    src_file (str): path to the input riegeli format file
    dest_file (str): path to the output JSON format file
  """
  with open(dest_file, 'w') as f:
    f.write('[\n')
    needs_comma = False
    reader = riegeli.RecordReader(io.FileIO(src_file, mode='rb'),)
    for problem in reader.read_messages(contest_problem_pb2.ContestProblem):
      if needs_comma:
        f.write(',\n')
      write_problem_as_json(problem, f)
      needs_comma = True
    f.write('\n]')
  print('{} => {}'.format(src_file, dest_file))


def convert_to_json(src_folder, dest_folder):
  """Iterates over all the riegeli format files in a folder and outputs them as a JSON file
  
  Args:
    src_folder (str): source folder containing the riegeli format files
    dest_folder (str): destination folder that will contain the JSON format files
  """
  for file_name in os.listdir(src_folder):
    if file_name.endswith(".riegeli"):  # validation and test files
      from_file = os.path.join(src_folder, file_name)
      to_file = os.path.join(dest_folder, file_name[:-7]+'json')
      convert_file_to_json(from_file, to_file)
    elif 'riegeli-' in file_name:  # training files
      from_file = os.path.join(src_folder, file_name)
      to_file = os.path.join(dest_folder, file_name.replace("riegeli-", "") + '.json')
      convert_file_to_json(from_file, to_file)


if __name__ == '__main__':
  convert_to_json(sys.argv[1], sys.argv[2])
